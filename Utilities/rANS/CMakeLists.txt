# Copyright 2019-2020 CERN and copyright holders of ALICE O2.
# See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
# All rights not expressly granted are reserved.
#
# This software is distributed under the terms of the GNU General Public
# License v3 (GPL Version 3), copied verbatim in the file "COPYING".
#
# In applying this license CERN does not waive the privileges and immunities
# granted to it by virtue of its status as an Intergovernmental Organization
# or submit itself to any jurisdiction.

o2_add_library(rANS
               TARGETVARNAME targetName
               SOURCES src/FrequencyTable.cxx
                       src/RenormedFrequencyTable.cxx
               PUBLIC_LINK_LIBRARIES FairLogger::FairLogger)
#target_compile_definitions(${targetName} PUBLIC O2_RANS_PRINT_PROCESSED_DATA)

o2_add_test(Iterators
            NAME Iterators
            SOURCES test/test_ransIterators.cxx
            PUBLIC_LINK_LIBRARIES O2::rANS
            COMPONENT_NAME rANS
            LABELS utils)

o2_add_test(HistogramView
            NAME HistogramView
            SOURCES test/test_ransHistogramView.cxx
            PUBLIC_LINK_LIBRARIES O2::rANS
            COMPONENT_NAME rANS
            LABELS utils)

o2_add_test(FrequencyTable
            NAME FrequencyTable
            SOURCES test/test_ransFrequencyTable.cxx
            PUBLIC_LINK_LIBRARIES O2::rANS
            COMPONENT_NAME rANS
            LABELS utils)

o2_add_test(SymbolTable
            NAME SymbolTable
            SOURCES test/test_ransSymbolTable.cxx
            PUBLIC_LINK_LIBRARIES O2::rANS
            COMPONENT_NAME rANS
            LABELS utils)

o2_add_test(ReverseSymbolLookupTable
            NAME ReverseSymbolLookupTable
            SOURCES test/test_ransReverseSymbolLookupTable.cxx
            PUBLIC_LINK_LIBRARIES O2::rANS
            COMPONENT_NAME rANS
            LABELS utils)

o2_add_test(EncodeDecode
            NAME EncodeDecode
            SOURCES test/test_ransEncodeDecode.cxx
            PUBLIC_LINK_LIBRARIES O2::rANS
            COMPONENT_NAME rANS
            LABELS utils)

if (TARGET benchmark::benchmark)
o2_add_executable(CombinedIterator
                    SOURCES benchmarks/bench_ransCombinedIterator.cxx
                    COMPONENT_NAME rANS
              IS_BENCHMARK
                    PUBLIC_LINK_LIBRARIES O2::rANS benchmark::benchmark)
endif()

o2_add_executable(rans-encode-decode-8
                  TARGETVARNAME targetName
                  SOURCES run/bin-encode-decode.cxx
                  PUBLIC_LINK_LIBRARIES O2::rANS Boost::program_options)
target_compile_definitions(${targetName} PRIVATE -DSOURCE_T=uint8_t)

o2_add_executable(rans-encode-decode-16
                  TARGETVARNAME targetName
                  SOURCES run/bin-encode-decode.cxx
                  PUBLIC_LINK_LIBRARIES O2::rANS Boost::program_options)
target_compile_definitions(${targetName} PRIVATE -DSOURCE_T=uint16_t)

o2_add_executable(rans-encode-decode-32
                  TARGETVARNAME targetName
                  SOURCES run/bin-encode-decode.cxx
                  PUBLIC_LINK_LIBRARIES O2::rANS Boost::program_options)
target_compile_definitions(${targetName} PRIVATE -DSOURCE_T=uint32_t)
