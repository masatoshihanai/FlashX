# Copyright 2014 Open Connectome Project (http://openconnecto.me)
# Written by Disa Mhembere (disa@jhu.edu)
#
# This file is part of FlashGraph.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include ../../Makefile.common

OMP_FLAG = -fopenmp
LDFLAGS := -L../matrix -lmatrix -L../libgraph-algs -L.. -lgraph -L../../libsafs -lsafs -lrt $(OMP_FLAG) -lz $(LDFLAGS)
CXXFLAGS += -I../../libsafs -I.. -I. $(OMP_FLAG)

all: test_matrix

test_matrix: test_matrix.o ../libgraph.a ../matrix/libmatrix.a
	$(CXX) -o test_matrix test_matrix.o $(LDFLAGS)

clean:
	rm -f *.d
	rm -f *.o
	rm -f *~
	rm -f test_matrix

-include $(DEPS) 
