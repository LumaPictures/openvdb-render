# OpenVDB Render

**What is this project?**

OpenVDB Render is a generic toolkit for rendering and visualizing OpenVDB volumes across multiple packages and 3rd party renderers.

**Where are we right now?**

The repository contains a Maya plugin, MtoA extension and a set of Arnold shaders. Even though some of the components, like visualisation, are simple, all the tools are production proven and actively used at Luma Pictures.

**What are our short-term goals?**

-   Improve real-time display in Maya
-   Windows support

**Supported platforms**

-   NVidia GPUs
-   Linux
-   Maya 2016 (OpenGL Core Profile) + 2016 Ext 2 + 2017
-   CY2017
-   Arnold 4.2.x
-   GCC 4.8.3
-   MtoA 1.2 and 1.4
-   Katana 2.5
-   Arnold 5 and MtoA-2.0 support is in the works, and will be released the same time as Arnold 5 / MtoA 2

**Notes**

If you are interested in the automatic grid discovery for KtoA or the early Arnold 5 work, and you have access to KtoA's source code or Arnold 5, please contact one of the repo maintainers for a patch.