:: It is helper for compilation shaders to SPIR-V

set PATH=C:\Users\elvir\msvc\glslang\bin;%PATH%

cd %~dp0
  set CFLAGSV= --target-env vulkan1.1 -V130 -d -t --aml --nsf -DAMD_PLATFORM -DUSE_F32_BVH -DENABLE_AMD_INSTRUCTION_SET -DUSE_MORTON_32

set INDIR=.\
set OUTDIR=..\build\shaders\amd\
set HRDDIR=..\include\vRt\HardCodes\amd\
set OUTSHR=..\build\shaders\

call shaders-list.cmd

pause
