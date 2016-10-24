# OpenVDB Render Contribution guide

**Coding Style**

The code follows the Allman style, with underscore non-capitalized naming scheme and using four spaces to ident the code. You are allowed to name classes that interface with external applications in the style of that application's SDK. CMake formatting follows the same rules; function calls are non-capitalized.

The docs subfolder includes a CLion code formatting preset, named luma_style.xml.

If you are developing code, make sure to follow these simple rules.

-   Always develop with all warnings enabled, and treat them an error.
-   Be const correct wherever you can.
-   Don't use "using namespace".
-   Minimize macro usage.
-   Minimize bringing in new dependencies, especially from boost. Boost::regex is allowed, since gcc 4.8's regex is broken.
-   Don't hardcode paths in CMake files.
-   Before creating a pull request, check your changes with clang's static analyzer.
