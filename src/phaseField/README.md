![](logo_v2.png)

[![Build Status](https://travis-ci.org/prisms-center/phaseField.svg?branch=master)](https://travis-ci.org/prisms-center/phaseField)
[![License: LGPL v2.1](https://img.shields.io/badge/License-lgpl-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)
## Useful Links:

[PRISMS-PF Website](https://prisms-center.github.io/phaseField/) <br>
[Code repository](https://github.com/prisms-center/phaseField) <br>
[User manual (with installation instructions)](https://prisms-center.github.io/phaseField/doxygen_files/manual.html) <br>
[User registration link](http://goo.gl/forms/GXo7Im8p2Y) <br>
[User forum](https://groups.google.com/forum/#!forum/prisms-pf-users) <br>
[Training slides/exercises](https://goo.gl/BBTkJ8)


## Version information:

This version of the code, v2.1, contains some somewhat substantial changes from v2.0.2. It was released in August 2018. See [version_changes.md](version_changes.md) for details.

## What is PRISMS-PF?

PRISMS-PF is a powerful, massively parallel finite element code for conducting phase field and other related simulations of microstructural evolution.  The phase field method is commonly used for predicting the evolution if microstructures under a wide range of conditions and material systems. PRISMS-PF provides a simple interface for solving customizable systems of partial differential equations of the type commonly found in phase field models, and has 24 pre-built application modules, including for precipitate evolution, grain growth, and solidification.

With PRISMS-PF, you have access to adaptive meshing and parallelization with near-ideal scaling for over a thousand processors. Moreover, the matrix-free framework from the deal.II library allows much larger than simulations than typical finite element programs – PRISMS-PF has been used for simulations with over one billion degrees of freedom. PRISMS-PF also provides performance competitive with or exceeding single-purpose codes. For example, even without enabling the mesh adaptivity features in PRISMS-PF, it has been demonstrated to be over 6x faster than an equivalent finite difference code.

This code is developed by the PRedictive Integrated Structural Materials Science (PRISMS) Center
at University of Michigan which is supported by the U.S. Department of Energy (DOE), Office of Basic Energy Sciences, Division of Materials Sciences and Engineering under Award #DE-SC0008637.

## Quick Start Guide:

For detailed instructions on how to download and use PRISMS-PF, please consult the [PRISMS-PF User Manual](https://prisms-center.github.io/phaseField/doxygen_files/manual.html). A (very) abbreviated version of the instructions is given below.

### Installation:

Please refer to the [installation section of the user manual](https://prisms-center.github.io/phaseField/doxygen_files/install.html) for details.

1) Install CMake, p4est, and deal.II (version 9.0.0 recommended)<br>

2) Clone the PRISMS-PF GitHub repository <br>
```
$ git clone https://github.com/prisms-center/phaseField.git
$ cd phaseField
$ git checkout master
$ cmake .
$ make -j nprocs
  ```
[here nprocs denotes the number of processors]

### Running a Pre-Built Application:

  Entering the following commands will run one of the pre-built example applications (the Cahn-Hilliard spinodal decomposition application in this case):<br>
  ```
  $ cd applications/cahnHilliard
  $ cmake .
  ```
  For debug mode [default mode, very slow]: <br>
  ```
  $ make debug
  ```
  For optimized mode:<br>
  ```
  $ make release
  ```
  Execution (serial runs): <br>
  ```
  $ ./main
  ```
  Execution (parallel runs): <br>
  ```
  $ mpirun -np nprocs main
  ```
  [here nprocs denotes the number of processors]

### Visualization:

  Output of the primal fields fields is in standard vtk
  format (parallel:*.pvtu, serial:*.vtu files) which can be visualized with the
  following open source applications:
  1. VisIt (https://wci.llnl.gov/simulation/computer-codes/visit/downloads)
  2. Paraview (http://www.paraview.org/download/)

### Getting started:

  Examples of various phase field models are located under the
  applications directory. The easiest way to get started on the code is to
  run the example apps in this folder.

  The example apps are intended to serve as (1) Demonstration of the
  capabilities of this library, (2) Provide a framework for
  further development of specialized/advanced applications by
  users.

  Apps that are still under development/testing are preceded by an
  underscore.

## License:

  GNU Lesser General Public License (LGPL). Please see the file
  LICENSE for details.

## Further information, questions, issues and bugs:

 + prisms-pf-users@googlegroups.com (user forum)
 + prismsphaseField.dev@umich.edu  (developer email list)
