file(GLOB_RECURSE sources *.c)
message("Build sources: ${sources}")

set(includes "." "display" "keyboard" "sensors" "calc")

idf_component_register(SRCS  ${sources}
                    INCLUDE_DIRS ${includes})
