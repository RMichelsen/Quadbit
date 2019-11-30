if not exist "../../../../Bin/Release/x64/Resources/Shaders/Compiled" mkdir "../../../../Bin/Release/x64/Resources/Shaders/Compiled"
if not exist "../../../../Bin/Debug/x64/Resources/Shaders/Compiled" mkdir "../../../../Bin/Debug/x64/Resources/Shaders/Compiled"

glslangValidator.exe	 -V voxel_frag.glsl					-S frag		-o Compiled/voxel_frag.spv
glslangValidator.exe	 -V voxel_frag.glsl					-S frag		-o ../../../../Bin/Release/x64/Resources/Shaders/Compiled/voxel_frag.spv
glslangValidator.exe	 -V voxel_frag.glsl					-S frag		-o ../../../../Bin/Debug/x64/Resources/Shaders/Compiled/voxel_frag.spv
							
glslangValidator.exe	 -V voxel_vert.glsl					-S vert		-o Compiled/voxel_vert.spv
glslangValidator.exe	 -V voxel_vert.glsl					-S vert		-o ../../../../Bin/Release/x64/Resources/Shaders/Compiled/voxel_vert.spv
glslangValidator.exe	 -V voxel_vert.glsl					-S vert		-o ../../../../Bin/Debug/x64/Resources/Shaders/Compiled/voxel_vert.spv