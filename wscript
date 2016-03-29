# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('aimf', ['internet'])
    module.source = [
        'model/aimf-header.cpp',
        'helper/aimf-helper.cpp',
        'model/aimf-routing-protocol.cpp',
        'model/aimf-state.cpp',
        ]

    module_test = bld.create_ns3_module_test_library('aimf')
    module_test.source = [
        'test/aimf-test-suite.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'aimf'
    headers.source = [
        'model/aimf-header.h',
        'helper/aimf-helper.h',
        'model/aimf-repository.h',
        'model/aimf-routing-protocol.h',
        'model/aimf-state.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

