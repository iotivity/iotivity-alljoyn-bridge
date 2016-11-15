#******************************************************************
#
# Copyright 2016 Intel Corporation All Rights Reserved.
#
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

vars = Variables('BuildOptions.txt')
vars.Add(EnumVariable('BUILD_TYPE', 'Specify release or debug build', 'debug', ['debug','release']))
vars.Add(PathVariable('ALLJOYN_BASE', 'Location of the alljoyn project', None, PathVariable.PathAccept))
vars.Add(EnumVariable('ALLJOYN_LIB_TYPE', 'Specify release or debug build', 'release', ['debug','release']))
vars.Add(PathVariable('IOTIVITY_BASE', 'Location of the iotivity project', None, PathVariable.PathAccept))
vars.Add(EnumVariable('IOTIVITY_LIB_TYPE', 'Specify release or debug build', 'release', ['debug','release']))
vars.Add(EnumVariable('TARGET_ARCH', 'Target architecture', 'x86_64', ['x86_64']))
vars.Add(BoolVariable('VERBOSE', 'Show compilation', False))
vars.Add(BoolVariable('COLOR', 'Enable color in build diagnostics, if supported by compiler', False))
#vars.Add(EnumVariable('TEST', 'Run unit tests', '0', allowed_values=('0', '1')))

env = Environment(variables = vars);
Help('''
Build options can be specified using the command line, BuildOptions.txt or using
the SCONS_FLAGS environment variable 

Example of command line release build:

   $ scons BUILD_TYPE=release ALLJOYN_BASE=/path/to/ALLJOYN_BASE IOTIVITY_BASE=/path/to/IOTIVITY_BASE

The current options are:
''')
Help(vars.GenerateHelpText(env))

if env.get('VERBOSE') == False:
    env['CCCOMSTR'] = "Compiling $TARGET"
    env['SHCCCOMSTR'] = "Compiling $TARGET"
    env['CXXCOMSTR'] = "Compiling $TARGET"
    env['SHCXXCOMSTR'] = "Compiling $TARGET"
    env['LINKCOMSTR'] = "Linking $TARGET"
    env['SHLINKCOMSTR'] = "Linking $TARGET"
    env['ARCOMSTR'] = "Archiving $TARGET"
    env['RANLIBCOMSTR'] = "Indexing Archive $TARGET"

# currently this build script only supports linux build as we add other target OSs
# target_os will need to be replaced buy actual OSs
target_os = 'linux'
    
# these must agree with options used in building IoTivity
env['CPPDEFINES'] = ['ROUTING_EP']
env['CPPFLAGS'] = ['-Wall', '-Werror', '-fPIC']
env['CXXFLAGS'] = ['-std=c++11']
if target_os == 'linux':
    env.AppendUnique(CPPDEFINES = ['QCC_OS_GROUP_POSIX'])

if env.get('COLOR') == True:
    # If the gcc version is 4.9 or newer add the diagnostics-color flag
    # the adding diagnostics colors helps discover error quicker.
    gccVer = env['CCVERSION'].split('.')
    if int(gccVer[0]) > 4:
        env['CPPFLAGS'].append('-fdiagnostics-color');
    elif int(gccVer[0]) == 4 and int(gccVer[1]) >= 9:
        env['CPPFLAGS'].append('-fdiagnostics-color');

if env['BUILD_TYPE'] == 'debug':
    env.AppendUnique(CPPFLAGS = ['-g'])
else:
    env.AppendUnique(CCFLAGS = ['-Os'])
    env.AppendUnique(CPPDEFINES = ['NDEBUG'])

alljoyn_inc_paths = ['${ALLJOYN_BASE}/build/' + target_os + '/${TARGET_ARCH}/${ALLJOYN_LIB_TYPE}/dist/cpp/inc']
iotivity_resource_inc_paths = ['${IOTIVITY_BASE}/extlibs/tinycbor/tinycbor/src',
                               '${IOTIVITY_BASE}/extlibs/cjson',
	                       '${IOTIVITY_BASE}/resource/c_common/oic_malloc/include',
	                       '${IOTIVITY_BASE}/resource/c_common/oic_string/include',
                               '${IOTIVITY_BASE}/resource/c_common',
                               '${IOTIVITY_BASE}/resource/csdk/logger/include',
                               '${IOTIVITY_BASE}/resource/csdk/stack/include',
                               '${IOTIVITY_BASE}/resource/include',
                               '${IOTIVITY_BASE}/resource/oc_logger/include']

env['CPPPATH'] = ['${IOTIVITY_BASE}/extlibs']
env.AppendUnique(CPPPATH = alljoyn_inc_paths)
env.AppendUnique(CPPPATH = iotivity_resource_inc_paths)

env.AppendUnique(CPPPATH = ['#/include/'])

env.Replace(BUILD_DIR = 'out/' + target_os + '/${TARGET_ARCH}/${BUILD_TYPE}' )

# libraries
env['LIBPATH'] = ['${ALLJOYN_BASE}/build/' + target_os + '/${TARGET_ARCH}/${ALLJOYN_LIB_TYPE}/dist/cpp/lib',
                  '${IOTIVITY_BASE}/out/' + target_os + '/${TARGET_ARCH}/${IOTIVITY_LIB_TYPE}']

alljoynplugin_lib = env.SConscript('src/SConscript', variant_dir = env['BUILD_DIR'] + '/obj/bridge', exports= 'env', duplicate=0)
env.Install('${BUILD_DIR}/libs', alljoynplugin_lib)

# build examples
env.SConscript('examples/SConscript', variant_dir = env['BUILD_DIR'] + '/obj/examples', exports= ['env', 'iotivity_resource_inc_paths', 'alljoynplugin_lib'], duplicate=0)

# build tests
env.SConscript('test/SConscript', variant_dir = env['BUILD_DIR'] + '/obj/test', exports= ['env', 'iotivity_resource_inc_paths'], duplicate=0)
