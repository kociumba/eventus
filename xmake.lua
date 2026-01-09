add_rules("mode.debug", "mode.release")

set_languages("cxx23")

target("eventus")
    set_kind("headeronly")
    add_headerfiles("eventus.h", "eventus/eventus", {public = true})
    add_includedirs("eventus", "/", {public = true})


for _, file in ipairs(os.files("examples/*.cpp")) do
local name = path.basename(file)

target(name)
    set_kind("binary")
    set_default(false)
    add_files(file)
    add_deps("eventus")
    add_cxxflags("-Wall")
end