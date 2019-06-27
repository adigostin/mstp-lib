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
		float const _lr_padding;
		com_ptr<ID2D1Brush> _text_brush;
		D2D1_RECT_F _editorBounds;
		std::wstring _text;
		text_layout_with_metrics _text_layout;
		size_t _selection_origin_pos = 0;
		size_t _caret_pos;
		d2d_window* const _control;

	public:
		text_editor (d2d_window* control, IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, uint32_t fill_argb, uint32_t text_argb, const D2D1_RECT_F& rect, float lr_padding, std::string_view text)
			: _control(control)
			, _dwrite_factory(dwrite_factory)
			, _format(format)
			, _fill_argb(fill_argb)
			, _text_argb(text_argb)
			, _editorBounds(rect)
			, _lr_padding(lr_padding)
		{
			assert (rect.right - rect.left >= lr_padding);
			auto buffer_size_chars = MultiByteToWideChar (CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
			_text.resize (buffer_size_chars + 1);
			MultiByteToWideChar (CP_UTF8, 0, text.data(), (int)text.size(), _text.data(), buffer_size_chars);
			_text.resize (buffer_size_chars);
			_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
			/*
			_transform = GetTransformToProjectCoords(e);
			_horzAlignment = (e->GetHorzAlignmentPD() != nullptr) ? e->GetHorzAlignmentPD()->Get(e) : HorzAlignmentLeft;
			_vertAlignment = (e->GetVertAlignmentPD() != nullptr) ? e->GetVertAlignmentPD()->Get(e) : VertAlignmentTop;
			*/
			extend_editor_bounds();

			set_caret_pos (_text.size(), true);
			set_caret_screen_location_from_caret_pos();

			//_control->GetZoomOrOriginChanged().AddHandler (&TextEditor::OnZoomOrOriginChanged, this);

			invalidate ();
		}

		~text_editor()
		{
			//_control->GetZoomOrOriginChanged().RemoveHandler (&TextEditor::OnZoomOrOriginChanged, this);
			_control->hide_caret();
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

		void set_caret_screen_location_from_caret_pos()
		{
			auto offset = get_text_location();

			float x = 0;
			if (_caret_pos > 0)
				x = text_layout_with_metrics (_dwrite_factory, _format, { _text.data(), _caret_pos }).width();

			static constexpr float caret_width_not_aligned = 1.5f;
			float pixel_width = 96.0f / _control->dpi();
			auto caret_width = roundf(caret_width_not_aligned / pixel_width) * pixel_width;

			D2D1_RECT_F b;
			b.left = roundf ((offset.x + x - caret_width / 2) / pixel_width) * pixel_width;
			b.top = roundf (offset.y / pixel_width) * pixel_width;
			b.right = b.left + caret_width;
			b.bottom = roundf ((b.top + _text_layout.height()) / pixel_width) * pixel_width;
			b = align_to_pixel(b, _control->dpi());
			_control->show_caret(b, D2D1::ColorF(_text_argb & 0x00FF'FFFF));
		}
	/*
		// static
		void TextEditor::OnZoomOrOriginChanged (void* callbackArg, ID2DZoomableWindow* window)
		{
			window;  // Avoid Reporting unreferenced formal parameter
			auto editor = static_cast<TextEditor*>(callbackArg);
			editor->set_caret_screen_location_from_caret_pos();
		}
		*/
		// TODO: rename to get_byte_index_at
		size_t GetPosAtDLocation (D2D1_POINT_2F dLocation, bool* isInside)
		{
			auto textOffset = get_text_location();
	
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
				set_caret_screen_location_from_caret_pos ();
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
				set_caret_screen_location_from_caret_pos();
			}
		}
		
		virtual handled process_virtual_key_down (uint32_t virtualKey, UINT modifierKeysDown) override
		{
			#pragma region Left
			if (virtualKey == VK_LEFT)
			{
				if ((modifierKeysDown == 0) || (modifierKeysDown == MK_SHIFT))
				{
					bool keepSelectionOrigin = (modifierKeysDown == MK_SHIFT);
					set_caret_pos ((_caret_pos > 0) ? (_caret_pos - 1) : 0, keepSelectionOrigin);
					set_caret_screen_location_from_caret_pos ();
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
					set_caret_screen_location_from_caret_pos ();
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
					set_caret_screen_location_from_caret_pos ();
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
					set_caret_screen_location_from_caret_pos ();
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
						_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
						set_caret_pos (_caret_pos, false);
						extend_editor_bounds ();
						set_caret_screen_location_from_caret_pos ();
						invalidate();
					}
				}
				else
				{
					// delete selection
					size_t selectionStart = std::min (_caret_pos, _selection_origin_pos);
					size_t selectionEnd   = std::max (_caret_pos, _selection_origin_pos);
					_text.erase (selectionStart, selectionEnd - selectionStart);
					_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
					set_caret_pos (selectionStart, false);
					extend_editor_bounds ();
					set_caret_screen_location_from_caret_pos ();
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
						_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
						set_caret_pos (_caret_pos - 1, false);
						extend_editor_bounds ();
						set_caret_screen_location_from_caret_pos ();
						invalidate();
					}
				}
				else
				{
					// delete selection
					size_t selectionStart = std::min (_caret_pos, _selection_origin_pos);
					size_t selectionEnd   = std::max (_caret_pos, _selection_origin_pos);
					_text.erase (selectionStart, selectionEnd - selectionStart);
					_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
					set_caret_pos (selectionStart, false);
					extend_editor_bounds();
					set_caret_screen_location_from_caret_pos ();
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

					auto hMem = ::GlobalAlloc (GMEM_MOVEABLE, 2 * (charCount + 1)); assert(hMem);
					wchar_t* mem = (wchar_t*) ::GlobalLock(hMem); assert(mem);
					wcsncpy_s (mem, charCount + 1, _text.data() + selectionStart, charCount);
					mem[charCount] = 0;
					BOOL bRes = ::GlobalUnlock(hMem); assert (bRes || (GetLastError() == NO_ERROR));

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

					bool cut = ((modifierKeysDown == MK_CONTROL) && (virtualKey == 'X'))
						|| ((modifierKeysDown == MK_SHIFT) && (virtualKey == VK_DELETE));
					if (cut && putToClipboard)
					{
						// delete selection
						set_caret_pos (selectionStart, false);
						_text.erase (selectionStart, charCount);
						_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
						extend_editor_bounds();
						set_caret_screen_location_from_caret_pos();
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
						wchar_t* mem = (wchar_t*) ::GlobalLock(h);
						if (mem)
						{
							insert_text_over_selection(mem, wcslen(mem));
							::GlobalUnlock(h);
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
		
		void insert_text_over_selection (const wchar_t* textToInsert, size_t textToInsertCharCount)
		{
			if (_selection_origin_pos != _caret_pos)
			{
				// replace selection
				size_t selectionStart = std::min (_caret_pos, _selection_origin_pos);
				size_t selectionEnd   = std::max (_caret_pos, _selection_origin_pos);
				_text.erase (selectionStart, selectionEnd - selectionStart);
				_text.insert (selectionStart, textToInsert, textToInsertCharCount);
				_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
				set_caret_pos (selectionStart + textToInsertCharCount, false);
			}
			else
			{
				// insert char at caret pos
				_text.insert (_caret_pos, textToInsert, textToInsertCharCount);
				_text_layout = text_layout_with_metrics (_dwrite_factory, _format, _text);
				set_caret_pos (_caret_pos + textToInsertCharCount, false);
			}

			extend_editor_bounds ();
			set_caret_screen_location_from_caret_pos ();
			invalidate();
		}

		virtual handled process_character_key (uint32_t ch) override
		{
			if (ch >= 0x20)
			{
				if (ch >= 0x80)
					assert(false); // not implemented

				wchar_t c = (wchar_t) ch;
				insert_text_over_selection (&c, 1);
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

		void extend_editor_bounds()
		{
			auto newEditorBounds = _editorBounds;

			if (_text_layout.width() > _editorBounds.right - _editorBounds.left)
			{
				// extend bounds horizontally
				float dif = _text_layout.width() - (_editorBounds.right - _editorBounds.left);
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

			if (_text_layout.height() > _editorBounds.bottom - _editorBounds.top)
			{
				// extend bounds vertically
				/*
				switch (_vertAlignment)
				{
				case VertAlignmentTop:
				*/
					newEditorBounds.bottom = newEditorBounds.top + _text_layout.height();
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

		D2D1_POINT_2F get_text_location() const
		{
			return { _editorBounds.left + _lr_padding, _editorBounds.top };
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
			dc->FillRectangle (_editorBounds, fill_brush);

			auto textOffset = get_text_location();

			size_t selectionStartIndex = std::min (_selection_origin_pos, _caret_pos);
			size_t selectionEndIndex   = std::max (_selection_origin_pos, _caret_pos);
			if (selectionEndIndex != selectionStartIndex)
			{
				auto layoutTextBefore = text_layout_with_metrics (_dwrite_factory, _format, { _text.data(), selectionStartIndex });
				
				auto layoutSelectedText = text_layout_with_metrics (_dwrite_factory, _format, { _text.data() + selectionStartIndex, selectionEndIndex - selectionStartIndex});

				D2D1_RECT_F rect;
				rect.left = textOffset.x + layoutTextBefore.width();
				rect.top = textOffset.y;
				rect.right = rect.left + layoutSelectedText.width();
				rect.bottom = rect.top + layoutSelectedText.height();

				com_ptr<ID2D1SolidColorBrush> b;
				dc->CreateSolidColorBrush (D2D1::ColorF(D2D1::ColorF::LightBlue), &b);
				dc->FillRectangle (rect, b);
			}

			com_ptr<ID2D1SolidColorBrush> text_brush;
			dc->CreateSolidColorBrush (D2D1::ColorF(_text_argb & 0xFFFFFF, (_text_argb >> 24) / 255.0f), &text_brush);
			dc->DrawTextLayout (textOffset, _text_layout, text_brush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
		}

		virtual std::wstring_view wstr() const override final { return _text; }

		virtual void select_all() override
		{
			if ((_selection_origin_pos != 0) || (_caret_pos != _text.length()))
			{
				_selection_origin_pos = 0;
				set_caret_pos (_text.length(), true);
				set_caret_screen_location_from_caret_pos ();
				invalidate();
			}
		}

		virtual const D2D1_RECT_F& rect() const override { return _editorBounds; }
	};

	text_editor_factory_t* const text_editor_factory =
		[](auto... params) -> std::unique_ptr<text_editor_i>
		{ return std::make_unique<text_editor>(std::forward<decltype(params)>(params)...); };
}
