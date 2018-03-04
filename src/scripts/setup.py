from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

ex = [
	"vector",
	"op_string",
	"op_socket",
	"d_malloc",
	"interface"
]

cxx_ex = [
	["interface"]
]

extensions = []
for x in ex:
	extensions.append(Extension(
		'%s' % x, ["%s.pyx" % x],
		include_dirs=["../../include/"],
		libraries=["autogentoo"],
		library_dirs=["../.libs/"]
	))

setup(
	ext_modules=cythonize(extensions)
)