/*******************************************************************
*
* Copyright 2022 Montera Pty Ltd
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*******************************************************************/

/**
 * @file

  A default implementation of a command line executable for command line environments.

  @author Greg McCreath
  @since 0.0.10

 * Does very little apart from strip the first argument from a command line (which in C is the program name)
 * and passes the rest to the #bvm_main() function.
 */

#include "../h/bvm.h"

/**
 * Main entry point for command line program.
 *
 * @param argc the argument count
 * @param argv the arguments
 *
 * @return a VM exit code.  0 is all good.
 */
int main(int argc, char *argv[]) {

	/* move past the 'C' executable name */
	argv+=1;
	argc-=1;

	/* and defer control to the vm main function */
	return bvm_main(argc, argv);
}
