
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge/object_list.h"
#include "edge/edge.h"

namespace pg
{
	static constexpr LONG text_lr_padding = 3;
	static constexpr LONG title_lr_padding = 4;
	static constexpr LONG title_ud_padding = 2;

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("328BCEDB-8816-4A8A-95E3-A79368998CD8") ICustomPropertyEditor : IUnknown
	{
		// Returns S_OK if the user selected a value, S_FALSE if the user cancelled, or an error code.
		virtual HRESULT STDMETHODCALLTYPE ShowModal (HWND hWndParent, VARIANT* pvarSelectedValue, bool readOnly) = 0;
		virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("7482245A-0574-4ED8-82AC-09A9367E0800")ICustomPropertyEditorFactory : IUnknown
	{
		// Implementation must listen to the collection change events and
		// at least not crash when objects are removed from the list.
		// The caller is responsible to keep the list object alive until after this function returns.
		virtual HRESULT STDMETHODCALLTYPE CreateEditor (edge::IObjectList* objs, ICustomPropertyEditor** ppEditor) = 0;
	};

	enum class read_state { ok, multiple_values, read_exception };

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("0AC00E07-C962-4250-B543-38B6B0F18B4B") IPropertyGrid : IUnknown
	{
		virtual HWND HWnd() const noexcept = 0;
		virtual RECT Bounds() const noexcept = 0;
		virtual void SetBounds (const RECT& rect) noexcept = 0;
		virtual void SetBorderWidth (float widthDIPs) noexcept = 0;		
		virtual HRESULT STDMETHODCALLTYPE AddSection (edge::IObjectList* objList, bool showEmptySel, edge::string_convert_context_i* scc) = 0;
		virtual HRESULT STDMETHODCALLTYPE RemoveSection (edge::IObjectList* objList) = 0;
		virtual void clear_sections() = 0;

		virtual void set_read_only (bool read_only) = 0;
		virtual bool read_only() const = 0;
		STDMETHOD(GetValueText)(edge::IObjectList* section, IDispatch* object, DISPID prop, read_state* pState, BSTR* pbstrValueText) = 0;
		virtual bool editing_text() const = 0;
		virtual void expand_all() = 0;
		//struct creating_object_e : edge::cancelable_event<creating_object_e, std::unique_ptr<edge::object>, edge::object*, const edge::object_property*, const edge::concrete_type*> { };
		//virtual creating_object_e::subscriber creating_object() = 0;

		struct property_edited_args
		{
			MEMBERID prop;
			const edge::IObjectList& objects;
		};

		struct value_property_edited_args : property_edited_args
		{
			std::vector<wil::unique_variant> old_values;
			wil::unique_variant new_value;
			edge::string_convert_context_i* context;
		};

		struct object_property_edited_args : property_edited_args
		{
			std::vector<com_ptr<IDispatch>> old_values;
		};

		struct value_collection_property_edited_args : property_edited_args
		{
			size_t value_index;
			std::vector<std::string> old_values;
			std::string new_value;
			edge::string_convert_context_i* context;
		};

		//struct property_edited_e : edge::event<property_edited_e, property_edited_args&&> { };
		//virtual property_edited_e::subscriber property_changed() = 0;
		//
		//struct item_set_cursor_e : edge::cancelable_event<item_set_cursor_e, HCURSOR, const htresult&> { };
		//virtual item_set_cursor_e::subscriber item_set_cursor() = 0;
		//
		//struct item_clicked_e : edge::cancelable_event<item_clicked_e, std::optional<LRESULT>, const htresult&> { };
		//virtual item_clicked_e::subscriber item_clicked() = 0;

	};

	HRESULT MakePropertyGrid (HWND hWnd, const RECT& bounds, edge::IThemeColorProvider* tp,
							  IPropertyGrid** ppGrid);
}
