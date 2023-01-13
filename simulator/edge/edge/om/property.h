#pragma once

namespace edge
{
	struct out_sstream_i
	{
		virtual void write (const char* data, size_t size) = 0;
		void write (std::string_view sv) { write (sv.data(), sv.size()); }
		void write (char ch) { write (&ch, 1); }
	};

	struct property
	{
		property() = default;
		property (const property&) = delete;
		property& operator= (const property&) = delete;
		virtual const char* name() const = 0;
	};

	struct property_change_args
	{
		const edge::property* const property;

	protected:
		property_change_args (const edge::property* property)
			: property(property)
		{ }

		// No reason to ever copy this. It's likely a programming error.
		property_change_args (const property_change_args&) = delete;
		property_change_args& operator= (const property_change_args&) = delete;

	public:
		virtual ~property_change_args() = default;
	};
}
