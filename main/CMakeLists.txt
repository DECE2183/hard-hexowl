file(GLOB_RECURSE sources *.c)
message("Build sources: ${sources}")

set(includes "." "display" "keyboard")

idf_component_register(SRCS  ${sources}
                    INCLUDE_DIRS ${includes})
