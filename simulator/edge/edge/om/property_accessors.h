#pragma once

namespace edge
{
	struct object;

	template<typename value_t>
	struct pointer_to_derived_member_var_t
	{
		using object_member_var_t = value_t(object::*);
		object_member_var_t const mv;

		constexpr pointer_to_derived_member_var_t (nullptr_t np) noexcept : mv(np) { }

		template<typename class_type> //requires std::is_base_of_v<object, class_type>
		constexpr pointer_to_derived_member_var_t (value_t(class_type::*mv)) noexcept
			: mv(static_cast<object_member_var_t>(mv))
		{ }

		operator object_member_var_t() const { return mv; }

		operator bool() const { return mv; }
	};

	template<typename return_t, bool const_, typename... args_t>
	struct pointer_to_derived_member_function_t
	{
		using object_member_fun_t = std::conditional_t<const_, return_t(object::*)(args_t...) const, return_t(object::*)(args_t...)>;
		object_member_fun_t const mg;

		constexpr pointer_to_derived_member_function_t() noexcept : mg(nullptr) { }

		constexpr pointer_to_derived_member_function_t (nullptr_t np) noexcept : mg(np) { }

		template<typename class_type> requires const_ //&& std::is_base_of_v<object, class_type>
			constexpr pointer_to_derived_member_function_t (return_t(class_type::*mg)(args_t...)const) noexcept
			: mg(static_cast<object_member_fun_t>(mg))
		{ }

		template<typename class_type> requires (const_ == false)//&& std::is_base_of_v<object, class_type>
			constexpr pointer_to_derived_member_function_t (return_t(class_type::*mg)(args_t...)) noexcept
			: mg(static_cast<object_member_fun_t>(mg))
		{ }

		operator object_member_fun_t() const { return mg; }

		operator bool() const { return mg; }
	};
}
