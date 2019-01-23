#include "pch.h"
#include "text_editor.h"
#include "utility_functions.h"

namespace edge
{
	class text_editor : public text_editor_i
	{
		com_ptr<IDWriteFactory> const _dwrite_factory;
		com_ptr<IDWriteTextFormat> const _format;
		uint32_t const _fill_argb;
		uint32_t const _text_argb;
		com_ptr<ID2D1Brush> _text_brush;
		D2D1_RECT_F _editorBounds;
		std::wstring _text;
		com_ptr<IDWriteTextLayout> _text_layout;
		size_t _selection_origin_pos = 0;
		size_t _caret_pos;
		d2d_window* const _control;

	public:
		text_editor (d2d_window* control, IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, uint32_t fill_argb, uint32_t text_argb, const D2D1_RECT_F& rect, std::string_view text)
			: _control(control)
			, _dwrite_factory(dwrite_factory)
			, _format(format)
			, _fill_argb(fill_argb)
			, _text_argb(text_argb)
			, _editorBounds(rect)
		{
			auto buffer_size_chars = MultiByteToWideChar (CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
			_text.resize (buffer_size_chars + 1);
			MultiByteToWideChar (CP_UTF8, 0, text.data(), (int)text.size(), _text.data(), buffer_size_chars);
			_text.resize (buffer_size_chars);
			auto hr = _dwrite_factory->CreateTextLayout (_text.data(), (UINT32)_text.size(), _format, 10000, 10000, &_text_layout); assert(SUCCEEDED(hr));
			/*
			_transform = GetTransformToProjectCoords(e);
			_horzAlignment = (e->GetHorzAlignmentPD() != nullptr) ? e->GetHorzAlignmentPD()->Get(e) : HorzAlignmentLeft;
			_vertAlignment = (e->GetVertAlignmentPD() != nullptr) ? e->GetVertAlignmentPD()->Get(e) : VertAlignmentTop;
			*/
			CalculateTextAndEditorBounds();

			set_caret_pos (_text.size(), true);
			SetCaretScreenLocationFromCaretIndex();
			_control->ShowCaret();

			//_control->GetZoomOrOriginChanged().AddHandler (&TextEditor::OnZoomOrOriginChanged, this);

			invalidate ();
		}

		~text_editor()
		{
			//_control->GetZoomOrOriginChanged().RemoveHandler (&TextEditor::OnZoomOrOriginChanged, this);
			_control->HideCaret();
		}

		void set_caret_pos (size_t pos, bool keepSelectionOrigin)
		{
			assert ((pos >= 0) && (pos <= _text.size()));

			if (keepSelectionOrigin == false)
			{
				if (_selection_origin_pos != _caret_pos)
					// There was some selection, now there will be none, so invalidate.
					invalidate ();

				if (_caret_pos != pos)
				{
					_caret_pos = pos;
				}

				_selection_origin_pos = _caret_pos;
			}
			else
			{
				if (_caret_pos != pos)
				{
					_caret_pos = pos;
					invalidate ();
				}
			}
		}

		void SetCaretScreenLocationFromCaretIndex()
		{
			HRESULT hr;

			auto offset = GetTextOffset();

			float x = 0;
			if (_caret_pos > 0)
			{
				com_ptr<IDWriteTextLayout> layout_before_caret;
				hr = _dwrite_factory->CreateTextLayout (_text.data(), (UINT32)_caret_pos, _format, 10000, 10000, &layout_before_caret); assert(SUCCEEDED(hr));
				DWRITE_TEXT_METRICS metrics;
				hr = layout_before_caret->GetMetrics (&metrics); assert(SUCCEEDED(hr));
				x = metrics.width;
			}

			DWRITE_TEXT_METRICS metrics;
			hr = _text_layout->GetMetrics(&metrics); assert(SUCCEEDED(hr));

			static constexpr float width = 2;
			D2D1_RECT_F b;
			b.left = offset.x + x - width / 2;
			b.top = offset.y;
			b.right = b.left + width;
			b.bottom = b.top + metrics.height;
			_control->SetCaretBounds (b);
		}
	/*
		// static
		void TextEditor::OnZoomOrOriginChanged (void* callbackArg, ID2DZoomableWindow* window)
		{
			window;  // Avoid Reporting unreferenced formal parameter
			auto editor = static_cast<TextEditor*>(callbackArg);
			editor->SetCaretScreenLocationFromCaretIndex();
		}
		*/
		// TODO: rename to get_byte_index_at
		size_t GetPosAtDLocation (D2D1_POINT_2F dLocation, bool* isInside)
		{
			auto textOffset = GetTextOffset();
	
			auto locationInEditorCoords = dLocation - textOffset;
	
			BOOL trailing, insideText;
			DWRITE_HIT_TEST_METRICS htm;
			auto hr = _text_layout->HitTestPoint (locationInEditorCoords.width, locationInEditorCoords.height, &trailing, &insideText, &htm); assert(SUCCEEDED(hr));
			auto pos = htm.textPosition;
			if (trailing)
				pos++;

			auto vertices = corners(_editorBounds);
			//VectorD verticesD[4];
			//_control->TransformLocationsToDisplayCoords(&vertices.points[0], verticesD, 4);

			auto insideBounds = point_in_polygon (vertices, dLocation);

			if(isInside != nullptr)
				*isInside = (insideBounds || insideText);

			return pos;
		}
	
		virtual void process_mouse_button_down (mouse_button button, UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip) override
		{
			if (button == mouse_button::left)
			{
				bool isInside;
				size_t byte_index = GetPosAtDLocation (dip, &isInside);

				bool keepSelectionOrigin = ((modifier_keys & MK_SHIFT) != 0);

				set_caret_pos (byte_index, keepSelectionOrigin);
				SetCaretScreenLocationFromCaretIndex ();
			}
		}

		virtual void process_mouse_button_up (mouse_button button, UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip) override
		{
		}

		virtual void process_mouse_move (UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip) override
		{
			if ((modifier_keys & MK_LBUTTON) != 0)
			{
				size_t pos = GetPosAtDLocation (dip, nullptr);
				set_caret_pos (pos, true);
				SetCaretScreenLocationFromCaretIndex();
			}
		}
		
		virtual handled process_virtual_key_down (uint32_t virtualKey, UINT modifierKeysDown) override
		{
			HRESULT hr;

			#pragma region Left
			if (virtualKey == VK_LEFT)
			{
				if ((modifierKeysDown == 0) || (modifierKeysDown == MK_SHIFT))
				{
					bool keepSelectionOrigin = (modifierKeysDown == MK_SHIFT);
					set_caret_pos ((_caret_pos > 0) ? (_caret_pos - 1) : 0, keepSelectionOrigin);
					SetCaretScreenLocationFromCaretIndex ();
					return handled::yes;
				}
			}
			#pragma endregion
			#pragma region Right
			else if (virtualKey == VK_RIGHT)
			{
				if ((modifierKeysDown == 0) || (modifierKeysDown == MK_SHIFT))
				{
					bool keepSelectionOrigin = (modifierKeysDown == MK_SHIFT);
					set_caret_pos ((_caret_pos < _text.length()) ? (_caret_pos + 1) : _text.length(), keepSelectionOrigin);
					SetCaretScreenLocationFromCaretIndex ();
					return handled::yes;
				}
			}
			#pragma endregion
			#pragma region Control + A
			else if ((modifierKeysDown == MK_CONTROL) && (virtualKey == 'A'))
			{
				select_all();
				return handled::yes;
			}
			#pragma endregion
			#pragma region Home
			else if (virtualKey == VK_HOME)
			{
				if ((modifierKeysDown == 0) || (modifierKeysDown == MK_SHIFT))
				{
					bool keepSelectionOrigin = (modifierKeysDown == MK_SHIFT);
					set_caret_pos (0, keepSelectionOrigin);
					SetCaretScreenLocationFromCaretIndex ();
					return handled::yes;
				}
			}
			#pragma endregion
			#pragma region End
			else if (virtualKey == VK_END)
			{
				if ((modifierKeysDown == 0) || (modifierKeysDown == MK_SHIFT))
				{
					bool keepSelectionOrigin = (modifierKeysDown == MK_SHIFT);
					set_caret_pos (_text.length(), keepSelectionOrigin);
					SetCaretScreenLocationFromCaretIndex ();
					return handled::yes;
				}
			}
			#pragma endregion
			#pragma region Del
			else if ((modifierKeysDown == 0) && (virtualKey == VK_DELETE))
			{
				if (_selection_origin_pos == _caret_pos)
				{
					// no selection. delete char right of the caret.
					if (_caret_pos < _text.length())
					{
						_text.erase (_caret_pos, 1);
						hr = _dwrite_factory->CreateTextLayout (_text.data(), (UINT32)_text.size(), _format, 10000, 10000, &_text_layout); assert(SUCCEEDED(hr));
						set_caret_pos (_caret_pos, false);
						CalculateTextAndEditorBounds ();
						SetCaretScreenLocationFromCaretIndex ();
						invalidate();
					}
				}
				else
				{
					// delete selection
					size_t selectionStart = std::min (_caret_pos, _selection_origin_pos);
					size_t selectionEnd   = std::max (_caret_pos, _selection_origin_pos);
					_text.erase (selectionStart, selectionEnd - selectionStart);
					hr = _dwrite_factory->CreateTextLayout (_text.data(), (UINT32)_text.size(), _format, 10000, 10000, &_text_layout); assert(SUCCEEDED(hr));
					set_caret_pos (selectionStart, false);
					CalculateTextAndEditorBounds ();
					SetCaretScreenLocationFromCaretIndex ();
					invalidate();
				}

				return handled::yes;
			}
			#pragma endregion
			#pragma region Back
			else if ((modifierKeysDown == 0) && (virtualKey == VK_BACK))
			{
				if (_selection_origin_pos == _caret_pos)
				{
					// no selection. delete char left of the caret.
					if (_caret_pos > 0)
					{
						_text.erase (_caret_pos - 1, 1);
						hr = _dwrite_factory->CreateTextLayout (_text.data(), (UINT32)_text.size(), _format, 10000, 10000, &_text_layout); assert(SUCCEEDED(hr));
						set_caret_pos (_caret_pos - 1, false);
						CalculateTextAndEditorBounds ();
						SetCaretScreenLocationFromCaretIndex ();
						invalidate();
					}
				}
				else
				{
					// delete selection
					size_t selectionStart = std::min (_caret_pos, _selection_origin_pos);
					size_t selectionEnd   = std::max (_caret_pos, _selection_origin_pos);
					_text.erase (selectionStart, selectionEnd - selectionStart);
					set_caret_pos (selectionStart, false);
					CalculateTextAndEditorBounds ();
					SetCaretScreenLocationFromCaretIndex ();
					invalidate();
				}

				return handled::yes;
			}
			#pragma endregion
			#pragma region Control + X / Shift + Del (Cut)   OR   Control + C / Control + Ins (Copy)
			else if (((modifierKeysDown == MK_CONTROL) && (virtualKey == 'X'))
				|| ((modifierKeysDown == MK_SHIFT) && (virtualKey == VK_DELETE))
				|| ((modifierKeysDown == MK_CONTROL) && (virtualKey == 'C'))
				|| ((modifierKeysDown == MK_CONTROL) && (virtualKey == VK_INSERT)))
			{
				if (_caret_pos != _selection_origin_pos)
				{
					size_t selectionStart = std::min (_caret_pos, _selection_origin_pos);
					size_t charCount = abs((int) _caret_pos - (int) _selection_origin_pos);

					auto hMem = GlobalAlloc (GMEM_MOVEABLE, 2 * (charCount + 1)); assert(hMem);
					wchar_t* mem = (wchar_t*) GlobalLock(hMem); assert(mem);
					wcsncpy_s (mem, charCount + 1, _text.data() + selectionStart, charCount);
					mem[charCount] = 0;
					BOOL bRes = GlobalUnlock(hMem); assert (bRes || (GetLastError() == NO_ERROR));

					bool putToClipboard = false;
					if (::OpenClipboard(_control->hwnd()))
					{
						if (::EmptyClipboard())
						{
							auto h = ::SetClipboardData (CF_UNICODETEXT, hMem);
							putToClipboard = (h != nullptr);
						}

						::CloseClipboard();
					}

					bool pressedCut = ((modifierKeysDown == MK_CONTROL) && (virtualKey == 'X')) || ((modifierKeysDown == MK_SHIFT) && (virtualKey == VK_DELETE));
					if (pressedCut && putToClipboard)
					{
						// delete selection
						_text.erase (selectionStart, charCount);
						set_caret_pos (selectionStart, false);
						CalculateTextAndEditorBounds();
						SetCaretScreenLocationFromCaretIndex();
						invalidate();
					}
				}

				return handled::yes;
			}
			#pragma endregion
			#pragma region Control + V / Shift + Ins (Paste)
			else if (((modifierKeysDown == MK_CONTROL) && (virtualKey == 'V'))
				|| ((modifierKeysDown == MK_SHIFT) && (virtualKey == VK_INSERT)))
			{
				if (::OpenClipboard(_control->hwnd()))
				{
					auto h = GetClipboardData(CF_UNICODETEXT);
					if (h != nullptr)
					{
						wchar_t* mem = (wchar_t*) GlobalLock(h);
						if (mem)
						{
							InsertTextOverwritingSelection(mem, wcslen(mem));
							GlobalUnlock(h);
						}
					}

					::CloseClipboard();
				}

				return handled::yes;
			}
			#pragma endregion

			return handled::no;
		}

		virtual handled process_virtual_key_up (uint32_t key, UINT modifier_keys) override
		{
			return handled::no;
		}
		
		void InsertTextOverwritingSelection (const wchar_t* textToInsert, size_t textToInsertCharCount)
		{
			HRESULT hr;

			if (_selection_origin_pos != _caret_pos)
			{
				// replace selection
				size_t selectionStart = std::min (_caret_pos, _selection_origin_pos);
				size_t selectionEnd   = std::max (_caret_pos, _selection_origin_pos);
				_text.erase (selectionStart, selectionEnd - selectionStart);
				_text.insert (selectionStart, textToInsert, textToInsertCharCount);
				hr = _dwrite_factory->CreateTextLayout (_text.data(), (UINT32)_text.size(), _format, 10000, 10000, &_text_layout); assert(SUCCEEDED(hr));
				set_caret_pos (selectionStart + textToInsertCharCount, false);
			}
			else
			{
				// insert char at caret pos
				_text.insert (_caret_pos, textToInsert, textToInsertCharCount);
				hr = _dwrite_factory->CreateTextLayout (_text.data(), (UINT32)_text.size(), _format, 10000, 10000, &_text_layout); assert(SUCCEEDED(hr));
				set_caret_pos (_caret_pos + textToInsertCharCount, false);
			}

			CalculateTextAndEditorBounds ();
			SetCaretScreenLocationFromCaretIndex ();
			invalidate();
		}

		virtual handled process_character_key (uint32_t ch) override
		{
			if (ch >= 0x20)
			{
				if (ch >= 0x80)
					assert(false); // not implemented

				wchar_t c = (wchar_t) ch;
				InsertTextOverwritingSelection (&c, 1);
				return handled::yes;
			}

			return handled::no;
		}
		void invalidate()
		{
			auto poly = corners(_editorBounds);
			//_transform.TransformLocations(&poly.points[0], 4);
			auto bounds = polygon_bounds(poly);
			_control->invalidate(bounds);
		}

		void CalculateTextAndEditorBounds()
		{
			DWRITE_TEXT_METRICS metrics;
			auto hr = _text_layout->GetMetrics (&metrics); assert(SUCCEEDED(hr));

			auto newEditorBounds = _editorBounds;

			if (metrics.width > _editorBounds.right - _editorBounds.left)
			{
				// extend bounds horizontally
				float dif = metrics.width - (_editorBounds.right - _editorBounds.left);
				/*
				if (_horzAlignment == HorzAlignmentLeft)
				{
				}
				else if (_horzAlignment == HorzAlignmentCenter)
				{
					newEditorBounds.location.x -= dif / 2;
				}
				else if (_horzAlignment == HorzAlignmentRight)
				{
					newEditorBounds.location.x -= dif;
				}
				else
					assert(false);
				*/
				newEditorBounds.right += dif;
			}

			if (metrics.height > _editorBounds.bottom - _editorBounds.top)
			{
				// extend bounds vertically
				/*
				switch (_vertAlignment)
				{
				case VertAlignmentTop:
				*/
					newEditorBounds.bottom = newEditorBounds.top + metrics.height;
				/*
					break;

				case VertAlignmentCenter:
				{
					float dif = layout->GetHeight() - _editorBounds.size.y;
					newEditorBounds.location.y -= dif / 2;
					newEditorBounds.size.y += dif;
					break;
				}

				case VertAlignmentBottom:
				case VertAlignmentBaseline:
				default:
					assert (false); // not implemented
				}
				*/
			}

			if (_editorBounds != newEditorBounds)
			{
				invalidate ();
				_editorBounds = newEditorBounds;
				invalidate ();
			}
		}

		D2D1_POINT_2F GetTextOffset() const
		{
			return { _editorBounds.left, _editorBounds.top };
			/*
			float x, y;
			switch (_horzAlignment)
			{
				case HorzAlignmentLeft:   x = _editorBounds.location.x; break;
				case HorzAlignmentCenter: x = _editorBounds.location.x + _editorBounds.size.x / 2 - layout->GetWidth() / 2; break;
				case HorzAlignmentRight:  x = _editorBounds.location.x + _editorBounds.size.x     - layout->GetWidth();     break;
				default: assert(false); x = 0;
			}

			switch (_vertAlignment)
			{
				case VertAlignmentTop:      y = _editorBounds.location.y; break;
				case VertAlignmentCenter:   y = _editorBounds.location.y + _editorBounds.size.y / 2 - layout->GetHeight() / 2; break;
				case VertAlignmentBottom:   y = _editorBounds.location.y + _editorBounds.size.y     - layout->GetHeight();     break;
				case VertAlignmentBaseline: y = _editorBounds.location.y + _editorBounds.size.y     - layout->GetBaseline();   break;
				default: assert(false); y = 0;
			}
	
			return { x, y };
			*/
		}

		virtual void render (ID2D1DeviceContext* dc) const override
		{
			com_ptr<ID2D1SolidColorBrush> fill_brush;
			dc->CreateSolidColorBrush (D2D1::ColorF(_fill_argb & 0xFFFFFF, (_fill_argb >> 24) / 255.0f), &fill_brush);

			auto textOffset = GetTextOffset();

			size_t selectionStartIndex = std::min (_selection_origin_pos, _caret_pos);
			size_t selectionEndIndex   = std::max (_selection_origin_pos, _caret_pos);
			if (selectionEndIndex != selectionStartIndex)
			{
				com_ptr<IDWriteTextLayout> layoutTextBefore;
				auto hr = _dwrite_factory->CreateTextLayout (_text.data(),
					(UINT32)selectionStartIndex, _format, 10000, 10000, &layoutTextBefore); assert(SUCCEEDED(hr));
				DWRITE_TEXT_METRICS layoutTextBefore_metrics;
				hr = layoutTextBefore->GetMetrics (&layoutTextBefore_metrics); assert(SUCCEEDED(hr));
				
				com_ptr<IDWriteTextLayout> layoutSelectedText;
				hr = _dwrite_factory->CreateTextLayout (_text.data() + selectionStartIndex,
					(UINT32)(selectionEndIndex - selectionStartIndex), _format, 10000, 10000, &layoutSelectedText); assert(SUCCEEDED(hr));
				DWRITE_TEXT_METRICS layoutSelectedText_metrics;
				hr = layoutSelectedText->GetMetrics (&layoutSelectedText_metrics); assert(SUCCEEDED(hr));

				D2D1_RECT_F rect;
				rect.left = textOffset.x + layoutTextBefore_metrics.width;
				rect.top = textOffset.y;
				rect.right = rect.left + layoutSelectedText_metrics.width;
				rect.bottom = rect.top + layoutSelectedText_metrics.height;

				com_ptr<ID2D1SolidColorBrush> b;
				dc->CreateSolidColorBrush (D2D1::ColorF(D2D1::ColorF::LightBlue), &b);
				dc->FillRectangle (rect, b);
			}

			com_ptr<ID2D1SolidColorBrush> text_brush;
			dc->CreateSolidColorBrush (D2D1::ColorF(_text_argb & 0xFFFFFF, (_text_argb >> 24) / 255.0f), &text_brush);
			dc->DrawTextLayout (textOffset, _text_layout, text_brush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
		}

		virtual std::string u8str() const override final
		{
			int char_count = WideCharToMultiByte (CP_UTF8, 0, _text.data(), (int)_text.size(), nullptr, 0, nullptr, nullptr);
			std::string str;
			str.resize (char_count);
			WideCharToMultiByte (CP_UTF8, 0, _text.data(), (int)_text.size(), str.data(), char_count, 0, nullptr);
			str.resize (str.size());
			return str;
		}

		virtual std::wstring_view wstr() const override final { return _text; }

		virtual void select_all() override
		{
			if ((_selection_origin_pos != 0) || (_caret_pos != _text.length()))
			{
				_selection_origin_pos = 0;
				set_caret_pos (_text.length(), true);
				SetCaretScreenLocationFromCaretIndex ();
				invalidate();
			}
		}


		virtual const D2D1_RECT_F& rect() const override { return _editorBounds; }
	};

	text_editor_factory_t* const text_editor_factory =
		[](auto... params) -> std::unique_ptr<text_editor_i>
		{ return std::make_unique<text_editor>(std::forward<decltype(params)>(params)...); };
}
