#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>

#include "test_utils.h"
#include "vbz.h"

#include "test_data.h"

#include <catch2/catch.hpp>

template <typename T>
void perform_compression_test(std::vector<T> const& data, CompressionOptions const& options)
{
    auto const          input_data_size = vbz_size_t(data.size() * sizeof(data[0]));
    std::vector<int8_t> dest_buffer(vbz_max_compressed_size(input_data_size, &options));
    auto final_byte_count = vbz_compress(data.data(), input_data_size, dest_buffer.data(),
                                         vbz_size_t(dest_buffer.size()), &options);
    REQUIRE(!vbz_is_error(final_byte_count));
    dest_buffer.resize(final_byte_count);

    std::vector<int8_t> decompressed_bytes(input_data_size);
    auto                decompressed_byte_count = vbz_decompress(
        dest_buffer.data(), vbz_size_t(dest_buffer.size()), decompressed_bytes.data(),
        vbz_size_t(decompressed_bytes.size()), &options);
    REQUIRE(!vbz_is_error(decompressed_byte_count));
    decompressed_bytes.resize(decompressed_byte_count);
    auto decompressed = gsl::make_span(decompressed_bytes).as_span<T>();

    // INFO("Original     " << dump_explicit<std::int64_t>(data));
    // INFO("Decompressed " << dump_explicit<std::int64_t>(decompressed));
    CHECK(decompressed == gsl::make_span(data));
}

template <typename T>
void run_compression_test_suite()
{
    GIVEN("Simple data to compress with no delta-zig-zag")
    {
        std::vector<T> simple_data(100);
        std::iota(simple_data.begin(), simple_data.end(), 0);

        CompressionOptions simple_options{false,  // no delta zig zag
                                          sizeof(T), 1, VBZ_DEFAULT_VERSION};

        perform_compression_test(simple_data, simple_options);
    }

    GIVEN("Simple data to compress and applying delta zig zag")
    {
        std::vector<T> simple_data(100);
        std::iota(simple_data.begin(), simple_data.end(), 0);

        CompressionOptions simple_options{true, sizeof(T), 1, VBZ_DEFAULT_VERSION};

        perform_compression_test(simple_data, simple_options);
    }

    GIVEN("Simple data to compress with delta-zig-zag and no zstd")
    {
        std::vector<T> simple_data(100);
        std::iota(simple_data.begin(), simple_data.end(), 0);

        CompressionOptions simple_options{true, sizeof(T), 0, VBZ_DEFAULT_VERSION};

        perform_compression_test(simple_data, simple_options);
    }

    GIVEN("Random data to compress")
    {
        std::vector<T> random_data(10 * 1000);
        auto           seed = std::random_device()();
        INFO("Seed " << seed);
        std::default_random_engine rand(seed);
        // std::uniform_int_distribution<std::int8_t> has issues on some platforms -
        // always use 32 bit engine
        std::uniform_int_distribution<std::int32_t> dist(std::numeric_limits<T>::min(),
                                                         std::numeric_limits<T>::max());
        for (auto& e : random_data)
        {
            e = dist(rand);
        }

        WHEN("Compressing with no delta zig zag")
        {
            CompressionOptions options{false, sizeof(T), 1, VBZ_DEFAULT_VERSION};
            perform_compression_test(random_data, options);
        }

        WHEN("Compressing with delta zig zag")
        {
            CompressionOptions options{true, sizeof(T), 0, VBZ_DEFAULT_VERSION};

            perform_compression_test(random_data, options);
        }

        WHEN("Compressing with zstd and delta zig zag")
        {
            CompressionOptions options{true, sizeof(T), 1, VBZ_DEFAULT_VERSION};

            perform_compression_test(random_data, options);
        }
    }
}

struct InputStruct
{
    std::uint32_t size     = 100;
    unsigned char keys[25] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
};

SCENARIO("vbz int8 encoding")
{
    run_compression_test_suite<std::int8_t>();
}

SCENARIO("vbz int16 encoding")
{
    run_compression_test_suite<std::int16_t>();
}

SCENARIO("vbz int32 encoding")
{
    run_compression_test_suite<std::int32_t>();
}

SCENARIO("vbz int32 known input data")
{
    GIVEN("A known input data set")
    {
        std::vector<std::int32_t> simple_data{5, 4, 3, 2, 1};

        WHEN("Compressed without zstd, with delta zig-zag")
        {
            CompressionOptions simple_options{true, sizeof(simple_data[0]), 0, VBZ_DEFAULT_VERSION};

            THEN("Data compresses/decompresses as expected")
            {
                perform_compression_test(simple_data, simple_options);
            }

            AND_WHEN("Checking compressed data")
            {
                auto const input_data_size
                    = vbz_size_t(simple_data.size() * sizeof(simple_data[0]));
                std::vector<int8_t> dest_buffer(
                    vbz_max_compressed_size(input_data_size, &simple_options));
                auto final_byte_count
                    = vbz_compress(simple_data.data(), input_data_size, dest_buffer.data(),
                                   vbz_size_t(dest_buffer.size()), &simple_options);
                dest_buffer.resize(final_byte_count);

                std::vector<int8_t> expected{
                    0, 0, 10, 1, 1, 1, 1,
                };

                INFO("Compressed   " << dump_explicit<std::int64_t>(dest_buffer));
                INFO("Decompressed " << dump_explicit<std::int64_t>(expected));
                CHECK(dest_buffer == expected);
            }
        }

        WHEN("Compressed with zstd and delta zig-zag")
        {
            CompressionOptions simple_options{true, sizeof(simple_data[0]), 100,
                                              VBZ_DEFAULT_VERSION};

            THEN("Data compresses/decompresses as expected")
            {
                perform_compression_test(simple_data, simple_options);
            }

            AND_WHEN("Checking compressed data")
            {
                auto const input_data_size
                    = vbz_size_t(simple_data.size() * sizeof(simple_data[0]));
                std::vector<int8_t> dest_buffer(
                    vbz_max_compressed_size(input_data_size, &simple_options));
                auto final_byte_count
                    = vbz_compress(simple_data.data(), input_data_size, dest_buffer.data(),
                                   vbz_size_t(dest_buffer.size()), &simple_options);
                dest_buffer.resize(final_byte_count);

                std::vector<int8_t> expected{
                    40, -75, 47, -3, 32, 7, 57, 0, 0, 0, 0, 10, 1, 1, 1, 1,
                };

                INFO("Compressed   " << dump_explicit<std::int64_t>(dest_buffer));
                INFO("Decompressed " << dump_explicit<std::int64_t>(expected));
                CHECK(dest_buffer == expected);
            }
        }
    }
}

SCENARIO("vbz int16 known input large data")
{
    GIVEN("Test data from a realistic dataset")
    {
        WHEN("Compressing with zig-zag deltas")
        {
            CompressionOptions options{true, sizeof(test_data[0]), 0, VBZ_DEFAULT_VERSION};

            perform_compression_test(test_data, options);
        }

        WHEN("Compressing with zstd")
        {
            CompressionOptions options{true, sizeof(test_data[0]), 1, VBZ_DEFAULT_VERSION};

            perform_compression_test(test_data, options);
        }

        WHEN("Compressing with no options")
        {
            CompressionOptions options{false, 1, 0, VBZ_DEFAULT_VERSION};

            perform_compression_test(test_data, options);
        }
    }
}

SCENARIO("vbz sized compression")
{
    GIVEN("A known input data set")
    {
        std::vector<std::int32_t> simple_data{5, 4, 3, 2, 1};

        WHEN("Compressed without zstd, with delta zig-zag")
        {
            CompressionOptions simple_options{true, sizeof(simple_data[0]), 0, VBZ_DEFAULT_VERSION};

            WHEN("Compressing data")
            {
                auto const input_data_size
                    = vbz_size_t(simple_data.size() * sizeof(simple_data[0]));
                std::vector<int8_t> compressed_buffer(
                    vbz_max_compressed_size(input_data_size, &simple_options));

                auto final_byte_count = vbz_compress_sized(
                    simple_data.data(), input_data_size, compressed_buffer.data(),
                    vbz_size_t(compressed_buffer.size()), &simple_options);
                compressed_buffer.resize(final_byte_count);

                THEN("Data is compressed correctly")
                {
                    std::vector<int8_t> expected{
                        20, 0, 0, 0, 0, 0, 10, 1, 1, 1, 1,
                    };

                    INFO("Compressed   " << dump_explicit<std::int64_t>(compressed_buffer));
                    INFO("Decompressed " << dump_explicit<std::int64_t>(expected));
                    CHECK(compressed_buffer == expected);
                }

                AND_WHEN("Decompressing data")
                {
                    std::vector<std::int8_t> dest_buffer(vbz_decompressed_size(
                        compressed_buffer.data(), vbz_size_t(compressed_buffer.size()),
                        &simple_options));
                    CHECK(dest_buffer.size() == input_data_size);

                    auto final_byte_count = vbz_decompress_sized(
                        compressed_buffer.data(), vbz_size_t(compressed_buffer.size()),
                        dest_buffer.data(), vbz_size_t(dest_buffer.size()), &simple_options);
                    CHECK(final_byte_count == input_data_size);

                    CHECK(gsl::make_span(dest_buffer).as_span<std::int32_t>()
                          == gsl::make_span(simple_data));
                }
            }
        }
    }
}

SCENARIO("my_flow_test_1", "[myflow1]")
{
    GIVEN("A small sample data vector")
    {
        // 创建一个简单的 int32_t 数据样例，用于测试压缩和解压缩流程
        std::vector<int32_t> sample_data{10, 20, 30, 40, 50};

        // 配置压缩选项：
        // delta_zig_zag = true 表示启用差分与zig-zag编码
        // element size 设置为 int32_t 的字节数
        // zstd_compression = 1 表示启用zstd压缩（非零即启用）
        // VBZ_DEFAULT_VERSION 是压缩版本号
        CompressionOptions options{true,  // delta zig zag enabled
                                   sizeof(sample_data[0]),
                                   1,  // enable zstd compression
                                   VBZ_DEFAULT_VERSION};

        WHEN("Compressing and decompressing the data")
        {
            // 输入数据大小（字节）
            auto input_size = vbz_size_t(sample_data.size() * sizeof(sample_data[0]));

            INFO("Original data size (bytes): " << input_size);
            INFO("Original data: " << dump_explicit<int32_t>(sample_data));

            // 为压缩输出分配足够的缓冲区
            std::vector<int8_t> compressed_buffer(vbz_max_compressed_size(input_size, &options));
            INFO("Allocated compressed buffer size: " << compressed_buffer.size());

            // 执行压缩
            auto compressed_size
                = vbz_compress(sample_data.data(), input_size, compressed_buffer.data(),
                               vbz_size_t(compressed_buffer.size()), &options);
            // 检查压缩是否成功
            REQUIRE(!vbz_is_error(compressed_size));
            compressed_buffer.resize(compressed_size);

            INFO("Compressed size (bytes): " << compressed_size);
            INFO("Compressed data: " << dump_explicit<int8_t>(compressed_buffer));

            // 准备解压缓冲区，大小等于原始输入大小
            std::vector<int8_t> decompressed_bytes(input_size);

            // 执行解压缩
            auto decompressed_size = vbz_decompress(
                compressed_buffer.data(), vbz_size_t(compressed_buffer.size()),
                decompressed_bytes.data(), vbz_size_t(decompressed_bytes.size()), &options);
            // 检查解压是否成功
            REQUIRE(!vbz_is_error(decompressed_size));
            decompressed_bytes.resize(decompressed_size);

            INFO("Decompressed size (bytes): " << decompressed_size);
            auto decompressed = gsl::make_span(decompressed_bytes).as_span<int32_t>();

            INFO("Decompressed data: " << dump_explicit<int32_t>(decompressed));

            THEN("The decompressed data should match the original")
            {
                // 验证解压后的数据是否与原始数据一致
                CHECK(decompressed == gsl::make_span(sample_data));
            }
        }
    }
}

SCENARIO("Compress and decompress int16_t data from binary file", "[myflow2]")
{
    using T                                  = int16_t;
    const int         zstd_compression_level = 5;
    const std::string input_file             = "/ssdData/reads_test_dat/reads_all.dat";
    const std::string output_file            = "./reads_all.dat.vbz";
    // const std::string input_file = "../../test_data/reads_test_dat/reads_10.dat";
    // const std::string output_file = "./reads_10.dat.vbz";
    // const std::string input_file = "../../test_data/reads_test_dat/reads_20.dat";
    // const std::string output_file = "./reads_20.dat.vbz";
    // const std::string input_file  = "../../test_data/reads_test_dat/reads_30.dat";
    // const std::string output_file = "./reads_30.dat.vbz";

    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Compression level: " << zstd_compression_level << std::endl;
    std::cout << "Onput file: " << output_file << std::endl;

    GIVEN("A binary file with int16_t data")
    {
        // 打开文件读取
        std::ifstream ifs(input_file, std::ios::binary);
        REQUIRE(ifs.is_open());

        // 读取数据到 vector
        std::vector<T> input_data((std::istreambuf_iterator<char>(ifs)),
                                  std::istreambuf_iterator<char>());
        ifs.close();

        // 字节数转换为元素个数
        REQUIRE(input_data.size() % sizeof(T) == 0);
        input_data.resize(input_data.size() / sizeof(T));
        INFO("Input data size: " << input_data.size());

        // 配置压缩选项
        CompressionOptions options{true,                    // delta zigzag
                                   sizeof(T),               // element size
                                   zstd_compression_level,  // enable zstd
                                   VBZ_DEFAULT_VERSION};

        WHEN("Compressing and decompressing the file data")
        {
            vbz_size_t input_size = vbz_size_t(input_data.size() * sizeof(T));

            // 分配压缩输出缓冲区
            std::vector<int8_t> compressed_buf(vbz_max_compressed_size(input_size, &options));

            auto start = std::chrono::high_resolution_clock::now();

            auto compressed_size
                = vbz_compress(input_data.data(), input_size, compressed_buf.data(),
                               vbz_size_t(compressed_buf.size()), &options);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration
                = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "Compression time: " << duration << " ms" << std::endl;

            REQUIRE(!vbz_is_error(compressed_size));
            compressed_buf.resize(compressed_size);

            // 压缩比计算
            double ratio = static_cast<double>(input_size) / static_cast<double>(compressed_size);
            std::cout << "Original bytes: " << input_size << std::endl;
            std::cout << "Compressed bytes: " << compressed_size << std::endl;
            std::cout << "Compression ratio: " << ratio << std::endl;

            // 写入输出文件
            std::ofstream ofs(output_file, std::ios::binary);
            REQUIRE(ofs.is_open());
            ofs.write(reinterpret_cast<const char*>(compressed_buf.data()), compressed_buf.size());
            ofs.close();

            THEN("Decompressed data should match the original")
            {
                std::vector<int8_t> decompressed_bytes(input_size);
                auto                decompressed_size = vbz_decompress(
                    compressed_buf.data(), vbz_size_t(compressed_buf.size()),
                    decompressed_bytes.data(), vbz_size_t(decompressed_bytes.size()), &options);
                REQUIRE(!vbz_is_error(decompressed_size));

                decompressed_bytes.resize(decompressed_size);
                auto decompressed = gsl::make_span(decompressed_bytes).as_span<T>();
                CHECK(decompressed == gsl::make_span(input_data));
            }
        }
    }
}
