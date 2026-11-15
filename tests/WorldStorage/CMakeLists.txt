add_executable(
	PalettedAnvilTest
	PalettedAnvilTest.cpp
	${PROJECT_SOURCE_DIR}/src/ChunkData.cpp
	${PROJECT_SOURCE_DIR}/src/WorldStorage/FastNBT.cpp
	${PROJECT_SOURCE_DIR}/src/WorldStorage/PalettedAnvil.cpp
)

target_include_directories(PalettedAnvilTest PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PalettedAnvilTest PRIVATE fmt::fmt)
set_target_properties(PalettedAnvilTest PROPERTIES FOLDER Tests/WorldStorage)
add_test(NAME PalettedAnvilTest COMMAND PalettedAnvilTest)
