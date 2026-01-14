add_rules("mode.debug", "mode.release")

set_languages("cxxlatest")

target("eventus")
    set_kind("headeronly")
    add_headerfiles("eventus.h", "eventus/eventus", {public = true})
    add_includedirs("eventus", "/", {public = true})

for _, file in ipairs(os.files("examples/*.cpp")) do
    local name = path.basename(file)

    target(name)
        set_kind("binary")
        set_group("examples")
        set_default(false)
        add_files(file)
        add_headerfiles("examples/**.h")
        add_deps("eventus")
        set_warnings("all", "extra")
end