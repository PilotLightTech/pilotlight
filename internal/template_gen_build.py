#-----------------------------------------------------------------------------
# [SECTION] imports
#-----------------------------------------------------------------------------

import os
import sys

try:
    import pl_build.core as pl
    import pl_build.backend_win32 as win32
    import pl_build.backend_linux as linux
    import pl_build.backend_macos as apple
except ImportError:
    # just use the packaged build so users don't need to pip install pl-build
    if len(sys.argv) > 1:
        sys.path.insert(0, sys.argv[1])
    import build.core as pl
    import build.backend_win32 as win32
    import build.backend_linux as linux
    import build.backend_macos as apple

# this will run all the builds
import gen_build_pilotlight
import gen_build_app
