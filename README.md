# OpenVDB Render

**What is this project?**

OpenVDB Render is a generic toolkit for rendering and visualizing openvdb volumes across multiple packages and 3rd party renderers.

**Where are we right now?**

The repository contains a Maya plugin, MtoA extension and a set of Arnold shaders. Even though some of the components, like visualisation, are simple, all the tools are production proven and actively used at Luma Pictures.

**What are our short-term goals?**

-   Improve real-time display in Maya.
-   Add support for Katana, and share as much code as possible between the Maya plugin and Katana.
-   Support building on Windows.

**Supported platforms**

-   Maya 2016 (OpenGL Core Profile)
-   CY2016
-   Arnold 4.2.x.
-   GCC 4.8.3.
