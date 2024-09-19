add_rules("mode.debug", "mode.release")

set_languages("c++23")

target("toyscheme")
    set_kind("binary")
    add_files("src/**.cpp")
    add_files("src/**.cppm")
