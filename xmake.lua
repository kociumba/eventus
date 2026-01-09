add_rules("mode.debug", "mode.release")

set_languages("cxx23")

target("eventus")
    set_kind("headeronly")
    add_headerfiles("eventus.h", "eventus/eventus", {public = true})
    add_includedirs("eventus", "/", {public = true})

target("eventus_test")
    set_kind("binary")
    set_default(false)
    add_files("tests/**.cpp")
    add_deps("eventus")
    add_cxxflags("-Wall")