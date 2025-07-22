#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "vbz.h"

template <typename T = int16_t>
bool vbz_compress_vector(const std::vector<T>& input_data,
                         std::vector<int8_t>&  compressed_out,
                         unsigned int          zstd_compression_level = 5,
                         bool                  perform_delta_zig_zag  = true)
{
    CompressionOptions options{perform_delta_zig_zag, sizeof(T), zstd_compression_level,
                               VBZ_DEFAULT_VERSION};

    vbz_size_t input_size_bytes = static_cast<vbz_size_t>(input_data.size() * sizeof(T));
    compressed_out.resize(vbz_max_compressed_size(input_size_bytes, &options));

    auto       start = std::chrono::high_resolution_clock::now();
    vbz_size_t compressed_size
        = vbz_compress(input_data.data(), input_size_bytes, compressed_out.data(),
                       static_cast<vbz_size_t>(compressed_out.size()), &options);
    auto end = std::chrono::high_resolution_clock::now();

    if (vbz_is_error(compressed_size))
    {
        std::cerr << "Error during compression" << std::endl;
        return false;
    }

    compressed_out.resize(compressed_size);

    std::cout << "[Compress] input size: " << input_size_bytes
              << " bytes, compressed size: " << compressed_size
              << " bytes, ratio: " << static_cast<double>(input_size_bytes) / compressed_size
              << ", time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms"
              << std::endl;

    return true;
}

template <typename T = int16_t>
bool vbz_decompress_vector(const std::vector<int8_t>& compressed_data,
                           std::vector<T>&            decompressed_out,
                           size_t                     original_element_count,
                           unsigned int               zstd_compression_level = 5,
                           bool                       perform_delta_zig_zag  = true)
{
    CompressionOptions options{perform_delta_zig_zag, sizeof(T), zstd_compression_level,
                               VBZ_DEFAULT_VERSION};

    vbz_size_t output_size_bytes = static_cast<vbz_size_t>(original_element_count * sizeof(T));
    std::vector<int8_t> decompressed_bytes(output_size_bytes);

    vbz_size_t decompressed_size
        = vbz_decompress(compressed_data.data(), static_cast<vbz_size_t>(compressed_data.size()),
                         decompressed_bytes.data(), output_size_bytes, &options);

    if (vbz_is_error(decompressed_size))
    {
        std::cerr << "Error during decompression" << std::endl;
        return false;
    }

    decompressed_bytes.resize(decompressed_size);
    const T* decompressed_data_ptr = reinterpret_cast<const T*>(decompressed_bytes.data());
    decompressed_out.assign(decompressed_data_ptr, decompressed_data_ptr + original_element_count);

    std::cout << "[Decompress] decompressed size: " << decompressed_size << " bytes" << std::endl;
    return true;
}

void test_file_vbz_compression(const std::string& input_file,
                               const std::string& output_file,
                               unsigned int       zstd_compression_level = 5,
                               bool               perform_delta_zig_zag  = true)
{
    using T = int16_t;

    std::cout << "\nInput file: " << input_file << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Compression level: " << zstd_compression_level << std::endl;
    std::cout << "Perform delta zig zag: " << perform_delta_zig_zag << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    // Open and read input file
    std::ifstream ifs(input_file, std::ios::binary);
    if (!ifs.is_open())
    {
        std::cerr << "Error: Could not open input file" << std::endl;
        return;
    }

    // Read data
    std::vector<T> input_data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    ifs.close();

    // Convert byte count to element count
    if (input_data.size() % sizeof(T) != 0)
    {
        std::cerr << "Error: Input data size is not a multiple of element size" << std::endl;
        return;
    }
    input_data.resize(input_data.size() / sizeof(T));

    // Set compression options
    CompressionOptions options{perform_delta_zig_zag,   // delta zigzag
                               sizeof(T),               // element size
                               zstd_compression_level,  // zstd compression level
                               VBZ_DEFAULT_VERSION};

    vbz_size_t input_size = vbz_size_t(input_data.size() * sizeof(T));

    // Allocate compression buffer
    std::vector<int8_t> compressed_buf(vbz_max_compressed_size(input_size, &options));

    // Compress
    auto start           = std::chrono::high_resolution_clock::now();
    auto compressed_size = vbz_compress(input_data.data(), input_size, compressed_buf.data(),
                                        vbz_size_t(compressed_buf.size()), &options);
    auto end             = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (vbz_is_error(compressed_size))
    {
        std::cerr << "Error during compression" << std::endl;
        return;
    }

    compressed_buf.resize(compressed_size);
    std::cout << "Compression time: " << duration << " ms" << std::endl;
    std::cout << "Original bytes: " << input_size << std::endl;
    std::cout << "Compressed bytes: " << compressed_size << std::endl;
    std::cout << "Compression ratio: " << (static_cast<double>(input_size) / compressed_size)
              << std::endl;

    // Write compressed data to file
    std::ofstream ofs(output_file, std::ios::binary);
    if (!ofs.is_open())
    {
        std::cerr << "Error: Could not open output file" << std::endl;
        return;
    }
    ofs.write(reinterpret_cast<const char*>(compressed_buf.data()), compressed_buf.size());
    ofs.close();

    // Decompress and verify
    std::vector<int8_t> decompressed_bytes(input_size);
    auto                decompressed_size = vbz_decompress(
        compressed_buf.data(), vbz_size_t(compressed_buf.size()), decompressed_bytes.data(),
        vbz_size_t(decompressed_bytes.size()), &options);

    if (vbz_is_error(decompressed_size))
    {
        std::cerr << "Error during decompression" << std::endl;
        return;
    }

    decompressed_bytes.resize(decompressed_size);

    // Compare decompressed data with original
    bool data_matches = true;
    if (decompressed_bytes.size() != input_size)
    {
        data_matches = false;
    }
    else
    {
        const T* decompressed_data = reinterpret_cast<const T*>(decompressed_bytes.data());
        for (size_t i = 0; i < input_data.size(); i++)
        {
            if (decompressed_data[i] != input_data[i])
            {
                data_matches = false;
                break;
            }
        }
    }

    if (data_matches)
    {
        std::cout << "Success: Decompressed data matches original" << std::endl;
    }
    else
    {
        std::cerr << "Error: Decompressed data does not match original" << std::endl;
    }
    std::cout << "========================================================" << std::endl;
}

void test_data_compress()
{
    auto print_vec = [](const std::string& tip, const auto& v) {
        std::cout << tip << ": [";
        for (size_t i = 0; i < v.size(); ++i)
        {
            std::cout << v[i];
            if (i + 1 < v.size())
                std::cout << ", ";
        }
        std::cout << "]\n";
    };

    std::vector<int16_t> raw_data = {10, 20, 30, 40, 50};
    std::vector<int8_t>  compressed;
    std::vector<int16_t> recovered;

    print_vec("raw_data", raw_data);

    if (!vbz_compress_vector(raw_data, compressed, 0, false))
    {
        return;
    }
    print_vec("compressed", compressed);
    if (!vbz_decompress_vector(compressed, recovered, raw_data.size(), 0, false))
    {
        return;
    }
    print_vec("recovered", recovered);
    assert(raw_data == recovered && "Mismatch between raw and recovered data!");
    std::cout << "================================================================\n";

    if (!vbz_compress_vector(raw_data, compressed, 1, false))
    {
        return;
    }
    print_vec("compressed", compressed);
    if (!vbz_decompress_vector(compressed, recovered, raw_data.size(), 1, false))
    {
        return;
    }
    print_vec("recovered", recovered);
    assert(raw_data == recovered && "Mismatch between raw and recovered data!");
    std::cout << "================================================================\n";

    if (!vbz_compress_vector(raw_data, compressed, 0, true))
    {
        return;
    }
    print_vec("compressed", compressed);
    if (!vbz_decompress_vector(compressed, recovered, raw_data.size(), 0, true))
    {
        return;
    }
    print_vec("recovered", recovered);
    assert(raw_data == recovered && "Mismatch between raw and recovered data!");
    std::cout << "================================================================\n";

    if (!vbz_compress_vector(raw_data, compressed, 1, true))
    {
        return;
    }
    print_vec("compressed", compressed);
    if (!vbz_decompress_vector(compressed, recovered, raw_data.size(), 1, true))
    {
        return;
    }
    print_vec("recovered", recovered);
    assert(raw_data == recovered && "Mismatch between raw and recovered data!");
    std::cout << "================================================================\n";
}

void test_file_data_compress()
{
    const std::string input_file = "../../test_data/reads_test_dat/reads_30.dat";
    test_file_vbz_compression(input_file, "./reads_reads_30.dat_1.vbz", 1);
    test_file_vbz_compression(input_file, "./reads_reads_30.dat_5.vbz", 5);
    test_file_vbz_compression(input_file, "./reads_reads_30.dat_9.vbz", 9);

    test_file_vbz_compression(input_file, "./reads_reads_30.dat_true.vbz", 1, true);
    test_file_vbz_compression(input_file, "./reads_reads_30.dat_true.vbz", 1, false);
}

int main()
{
    test_data_compress();
    // test_file_data_compress();
    return 0;
}
