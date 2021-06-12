import qbs

Project {
    minimumQbsVersion: "1.7"

    CppApplication {
        consoleApplication: true

        cpp.warningLevel: "all"
        cpp.treatWarningsAsErrors: true

        cpp.cxxLanguageVersion: "c++14"

        cpp.defines: [
            "_REENTRANT",
            "HAS_IEEE_FLOAT",
            "HAS_COSINE_TABLE"
        ]

        cpp.dynamicLibraries: [
            "pulse-simple",
            "pulse"
        ]

        cpp.includePaths: [
            "wav_decoder"
        ]

        files: [
            "audiotrack.cpp",
            "audiotrack.h",
            "cosine.cpp",
            "cosine.h",
            "cosine.py",
            "main.cpp",
            "wav_decoder/wavreader.cpp",
            "wav_decoder/wavreader.h",
        ]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
