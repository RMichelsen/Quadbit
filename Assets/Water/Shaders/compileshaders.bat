glslangValidator.exe	 -V water_frag.glsl				-S frag		-o Compiled/water_frag.spv						
glslangValidator.exe	 -V water_vert.glsl				-S vert		-o Compiled/water_vert.spv
glslangValidator.exe	 -V precalc_comp.glsl				-S comp		-o Compiled/precalc_comp.spv
glslangValidator.exe	 -V waveheight_comp.glsl			-S comp		-o Compiled/waveheight_comp.spv
glslangValidator.exe	 -V ifft_comp.glsl				-S comp		-o Compiled/ifft_comp.spv
glslangValidator.exe	 -V displacement_comp.glsl			-S comp		-o Compiled/displacement_comp.spv
