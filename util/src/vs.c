/* Copyright (c) 2010-2018 Sander Mertens
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 #include "../include/util.h"

char * ut_get_vs_dir()
{
    // Check application is executed from VS 2017 Command Prompt
    char * vs_path_env = ut_getenv("VSINSTALLDIR");
    if (!vs_path_env){
        // Check for user defined variable
        vs_path_env = ut_getenv("VS2017");
        if (!vs_path_env)
        {
            ut_log("#[yellow]Attension#[reset]   $VS2017 not defined. Using default Visaul Studio 2017 path.\n");
            vs_path_env = strdup("C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community");
        }
    }
    return vs_path_env;
}

char * ut_get_vc_shell_cmd()
{
    char * vs_path_env = ut_get_vs_dir();
    if (vs_path_env[strlen(vs_path_env) - 1] == '\\')
        vs_path_env[strlen(vs_path_env) - 1] = '\0';
    char * vc_shell = ut_asprintf("%s\\VC\\Auxiliary\\Build\\vcvarsall.bat", vs_path_env);
    // Check file path exist
    char * vc_shell_cmd = ut_asprintf("call \"%s\" x64", vc_shell);
    return vc_shell_cmd;
}

