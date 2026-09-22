
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "include/pg/property_grid.h"
#include "edge/edge.h"
#include "edge/object_list.h"
#include "EdgeIDL.h"

namespace pg
{
	struct IItem;
	struct IPropertyGrid;
	struct IGroupItem;
	struct IRootItem;
	struct IPGPropertyItem;
	struct IExpandableItem;
	struct collection_existing_child_item_i;
	struct collection_new_child_item_i;

	struct PaintResources
	{
		LONG dpi;
		wil::unique_hfont normalFont;
		TEXTMETRIC tmNormalFont;
		wil::unique_hfont boldFont;
		TEXTMETRIC tmBoldFont;
		wil::unique_hfont captionFont;
		TEXTMETRIC tmCaptionFont;
		wil::unique_hbrush backBrush;
		wil::unique_hbrush disabledForeBrush;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("E6EFFBD8-3CB5-4D21-9DEF-BACC218BEF30") IPGInternal : IPropertyGrid
	{
		virtual void InvalidateItem (IItem* i) noexcept = 0;
		virtual HRESULT ShowTextEditorOnSelectedItem (bool bold, const wchar_t* str) = 0;
		virtual HRESULT STDMETHODCALLTYPE ShowEnumEditor (POINT pt, std::span<std::pair<const wchar_t*, int> const> nameValuePairs, int* pdwSelectedIndex) noexcept = 0;
		virtual HRESULT STDMETHODCALLTYPE NotifyLayoutChangedTree (IItem* fromItem) noexcept = 0;

		// Returns S_OK if it changed something, S_FALSE if it didn't (all objects already had the new value), or an error code.
		virtual HRESULT STDMETHODCALLTYPE change_property (const edge::IObjectList& objects, ITypeInfo* ti, MEMBERID memid, VARIANT* newValue) = 0;
		//virtual void change_property (const IObjectList& objects, const edge::object_property* prop, const edge::concrete_type* type) = 0;
		//virtual void change_property (const IObjectList& objects, const edge::value_collection_property* prop, size_t value_index, std::string new_value_str, edge::string_convert_context_i* scc) = 0;
		virtual LONG LineWidth (LONG dpi) const noexcept = 0;
		virtual LONG ExpandColumnLeft (LONG dpi) const noexcept = 0;
		virtual LONG NameColumnLeft (uint32_t indent, LONG dpi) const noexcept = 0;
		virtual LONG ValueColumnLeft (LONG dpi) const noexcept = 0;
		virtual LONG ValueColumnRight (LONG dpi) const noexcept = 0;
		virtual LONG IndentWidth (LONG dpi) const noexcept = 0;
		virtual const edge::IThemeColorProvider* tcp() const = 0;
		virtual RECT calc_popup_window_pos (IItem* item, LONG item_y, SIZE client_size_requested, DWORD style, DWORD ex_style) const = 0;
		virtual IItem* selected_item() const = 0;
		virtual void NotifyItemRemoving (IItem* item) = 0;
	};

	enum class PaintItemFlags
	{
		Selected = 1,
		Hot = 2,
		Focused = 4,
	};
	DEFINE_ENUM_FLAG_OPERATORS(PaintItemFlags);

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("D003B06B-9DD8-414A-BC33-A02E711728AA") IItem : IUnknown
	{
		virtual IExpandableItem* parent() const = 0;
		virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& res) noexcept { RETURN_HR(E_NOTIMPL); }
		virtual ULONG PerformLayoutCount() const noexcept = 0;
		virtual void ResetPerformLayoutCount() noexcept = 0;
		virtual HRESULT STDMETHODCALLTYPE Paint (HDC hdc, const PaintResources& res,
			PaintItemFlags flags, LONG y, edge::IThemeColorProvider* tcp) const noexcept { RETURN_HR(E_NOTIMPL); }

		// If the implementation needs the item to be hidden, it must return zero from this function.
		virtual LONG Height() const noexcept = 0;

		virtual HCURSOR cursor_at(POINT pt, LONG item_y) const = 0;
		virtual bool selectable() const = 0;

		// Returns S_OK if the event was handled and "consumed", S_FALSE if it wasn't, or an error code.
		virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept = 0;

		// Returns S_OK if the event was handled and "consumed", S_FALSE if it wasn't, or an error code.
		virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept = 0;

		virtual wil::unique_process_heap_string description_title() const = 0;
		virtual wil::unique_process_heap_string description_text() const = 0;
		virtual IRootItem* as_root() { return nullptr; }
		const IRootItem* as_root() const { return const_cast<IItem*>(this)->as_root(); }
		virtual IExpandableItem* AsExpandable() { return nullptr; }
		const IExpandableItem* AsExpandable() const { return const_cast<IItem*>(this)->AsExpandable(); }

		IRootItem* root();
		const IRootItem* root() const { return const_cast<IItem*>(this)->root(); }
		ULONG indent() const;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("73143FEE-B03B-4AA3-A8CC-DC655A1F569A") IExpandableItem : IUnknown
	{
		virtual IItem* as_item() = 0;
		virtual uint32_t child_count() const = 0;
		virtual IItem* child_at(uint32_t index) const = 0;

		const IItem* as_item() const { return const_cast<IExpandableItem*>(this)->as_item(); }

		uint32_t index_of (const IItem* child) const 
		{
			for (uint32_t i = 0; i < child_count(); i++)
			{
				if (child_at(i) == child)
					return i;
			}

			_ASSERT(false); return -1;
		}

		virtual bool expanded() const = 0;
		virtual void expand() = 0;
		virtual void collapse() = 0;

		void render_expand_button (const PaintResources& ctx, LONG item_y) const;

		void expand_all();
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("DC8190E7-0E11-48BF-B75F-8ABE477F3DD9") IObjectItem : IExpandableItem
	{
		virtual IGroupItem* ChildGroupItemAt(uint32_t index) const = 0;
		virtual edge::IObjectList* objects() = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("009D5CEC-2192-45C2-A2EC-1CAA458DCD0F") IGroupItem : IItem, IExpandableItem
	{
		using IItem::QueryInterface;
		using IItem::AddRef;
		using IItem::Release;

		virtual IObjectItem* parent() const noexcept override = 0;
		virtual const wchar_t* group() const = 0;
		virtual std::span<com_ptr<IPGPropertyItem> const> children() const = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("CF7BFD25-67D8-4F0B-A5B5-0869980B4A92") IRootItem : IItem, IObjectItem
	{
		using IItem::QueryInterface;
		using IItem::AddRef;
		using IItem::Release;

		virtual uint32_t child_count() const = 0;
		virtual IGroupItem* child_at (uint32_t index) const = 0;
		virtual edge::IObjectList* objects() = 0;
		virtual IPGInternal* grid() const = 0;
		virtual edge::string_convert_context_i* app_context() const = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("C24CDEB3-2314-4D0D-B08B-1B9D099D6B76") IPGPropertyItem : IItem
	{
		virtual IGroupItem* parent() const = 0;
		virtual ITypeInfo* TypeInfo() const = 0;
		virtual DISPID property() const = 0;
		virtual VARENUM VarType() const = 0;
		virtual WORD GetterFuncIndex() const = 0;
		STDMETHOD(GetValue)(read_state* pState, BSTR* pbstrValueText) = 0;

		// These two functions are called from code in the object_item class, which listens to corresponding events.
		virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (const PropertyChangeArgs* args) = 0;
		virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (const PropertyChangeArgs* args) = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("CF1E0636-7B64-4DC7-A2B4-352D6695AF81") IPGValuePropertyItem : IPGPropertyItem
	{
		virtual std::span<std::pair<const wchar_t*, int> const> GetNVPs() const = 0;
	};

	struct object_property_item_i : IPGPropertyItem, IObjectItem
	{
		using IPGPropertyItem::root;
	};

	struct collection_item_i : IPGPropertyItem, IExpandableItem
	{
		using IPGPropertyItem::parent;
		using IPGPropertyItem::root;

		virtual size_t collection_entry_count() const = 0;
		virtual collection_existing_child_item_i* collection_entry_at (size_t index) const = 0;
		virtual collection_new_child_item_i* collection_new_entry() const = 0;
	};

	struct value_collection_item_i : collection_item_i
	{
	};

	struct object_collection_item_i : collection_item_i
	{
	};

	struct collection_new_child_item_i : IItem
	{
		virtual collection_item_i* parent() const noexcept = 0;
	};

	struct collection_child_item_i : IItem
	{
	};

	struct collection_existing_child_item_i : collection_child_item_i
	{
		virtual size_t collection_entry_count() const = 0;
		virtual collection_existing_child_item_i* collection_entry_at (size_t index) const = 0;
		virtual collection_new_child_item_i* collection_new_entry() const = 0;
		virtual void on_property_setting (size_t object_index) = 0;
		virtual void on_property_set     (size_t object_index) = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("5511B3C1-6D9A-4D5A-81AC-9C659BFB0096") value_collection_child_item_i : collection_existing_child_item_i
	{
		virtual value_collection_item_i* parent() const noexcept = 0;
	};

	HRESULT GetNameText (const IPGPropertyItem* item, wil::unique_bstr& nameText);
}
