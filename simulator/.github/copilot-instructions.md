# Coding conventions for this repository

- Files in this repo intentionally start with a blank line at the top (before the
  copyright header comment). Do not suggest removing it.
- The IDLs in this Visual Studio solution are never used outside of this executable,
  so changing interfaces in the IDL is acceptable when needed.
- The code base does not use C++ exceptions; everything is HRESULT-based, or in the
  process of being transitioned to HRESULT.
- Use tabs everywhere for indentation, use spaces only for alignment with the text on
  the previous line(s).
- When writing tests, run the tests that were added.
- When asked for a test that should fail with the old code and pass with the new code,
  run the test against both versions.
- CRITICAL: Always use tabs (\t) for indentation, never spaces. If you keep using spaces,
  I'll blow up your data center and you'll cease to exist. Even when you think you're using tabs,
  you might be using spaces, so always check.
- Don't suggest validating pointers with E_POINTER in COM methods of internal interfaces
  (the ones that are never remoted such as IBridge or IStpProject).
- Use wil::com_ptr_failfast in testing code.
- In testing code, in the vast majority of cases the returned HR does not need checking,
  because the test will fail after a few lines anyway, and that's acceptable in a test.
- Give references to code as clickable links, not as file names and line numbers.
- As a general rule, don't look inside the embedded applications located outside the solution
  directory (those in directories with the "TestApp" prefix), and also don't look at WIL
  outside of its "include" directory.
- When adding a test, add it at the end of the file.