include_guard(GLOBAL)

# TrackHat build: only the modules the TrackHat sensor needs are built.
function(otr_init_variant)
    set_property(GLOBAL PROPERTY opentrack-variant "default")
    set_property(GLOBAL PROPERTY opentrack-ident "trackhat-v3")

    set(subprojects
        "tracker-neuralnet"
        "proto-*"
        "filter-accela"
        "options"
        "api"
        "compat"
        "logic"
        "input"
        "gui"
        "main"
        "x-plane-plugin"
        "csv"
        "pose-widget"
        "head-test"
        "spline"
        "qxt-mini"
        "macosx"
        "cv"
        "migration"
        "main-window"
        "video"
        "video-opencv"
        "opentrack"
    )
    set_property(GLOBAL PROPERTY opentrack-subprojects "${subprojects}")
endfunction()
