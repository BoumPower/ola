# LIBRARIES
##################################################
if USE_I2C
# This is a library which isn't coupled to olad
lib_LTLIBRARIES += plugins/i2c/libolai2ccore.la plugins/i2c/libolai2c.la
plugins_i2c_libolai2ccore_la_SOURCES = \
    plugins/i2c/I2CBackend.cpp \
    plugins/i2c/I2CBackend.h \
    plugins/i2c/I2COutput.cpp \
    plugins/i2c/I2COutput.h \
    plugins/i2c/I2CWriter.cpp \
    plugins/i2c/I2CWriter.h
plugins_i2c_libolai2ccore_la_LIBADD = common/libolacommon.la

# Plugin description is generated from README.md
built_sources += plugins/i2c/I2CPluginDescription.h
nodist_plugins_i2c_libolai2c_la_SOURCES = \
    plugins/i2c/I2CPluginDescription.h
plugins/i2c/I2CPluginDescription.h: plugins/i2c/README.md plugins/i2c/Makefile.mk plugins/convert_README_to_header.sh
	sh $(top_srcdir)/plugins/convert_README_to_header.sh $(top_srcdir)/plugins/i2c $(top_builddir)/plugins/i2c/I2CPluginDescription.h

plugins_i2c_libolai2c_la_SOURCES = \
    plugins/i2c/I2CDevice.cpp \
    plugins/i2c/I2CDevice.h \
    plugins/i2c/I2CPlugin.cpp \
    plugins/i2c/I2CPlugin.h \
    plugins/i2c/I2CPort.cpp \
    plugins/i2c/I2CPort.h
plugins_i2c_libolai2c_la_LIBADD = \
    common/libolacommon.la \
    olad/plugin_api/libolaserverplugininterface.la \
    plugins/i2c/libolai2ccore.la

# TESTS
##################################################
test_programs += plugins/i2c/I2CTester

plugins_i2c_I2CTester_SOURCES = \
    plugins/i2c/I2CBackendTest.cpp \
    plugins/i2c/I2COutputTest.cpp \
    plugins/i2c/FakeI2CWriter.cpp \
    plugins/i2c/FakeI2CWriter.h
plugins_i2c_I2CTester_CXXFLAGS = $(COMMON_TESTING_FLAGS)
plugins_i2c_I2CTester_LDADD = $(COMMON_TESTING_LIBS) \
                              plugins/i2c/libolai2ccore.la \
                              common/libolacommon.la

endif

EXTRA_DIST += plugins/i2c/README.md
