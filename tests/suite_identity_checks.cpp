#include "../src/suite_identity.h"
#include <cstdlib>
#include <iostream>

static unsigned checks = 0;
static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
    ++checks;
}
static std::string identity(const std::string& bytes) {
    return rms_jar_identity(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}
static void expect(const std::string& bytes, const char* digest, const char* message) {
    const std::string actual = identity(bytes);
    require(actual == digest, message);
    require(actual.size() == 64 && actual.find_first_not_of("0123456789abcdef") == std::string::npos,
            "complete lowercase SHA-256");
}

int main() {
    expect("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
           "standard empty vector");
    require(rms_jar_identity(nullptr, 0) == identity(""), "null empty input");
    expect("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
           "standard abc vector");
    expect("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
           "standard two-block padding vector");
    expect(std::string(1000000, 'a'),
           "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
           "standard million-a vector");

    // Independently generated with Python 3 hashlib, not this SHA implementation:
    // hashlib.sha256(bytes((i * 37 + 11) % 256 for i in range(n))).hexdigest()
    struct Vector { size_t size; const char* digest; };
    const Vector vectors[] = {
        {1, "e7cf46a078fed4fafd0b5e3aff144802b853f8ae459a4f0c14add3314b7cc3a6"},
        {54, "0b35cded48f546833dc4e93133ffc3d7e05c7e41842f4accc39c6e04204c8dde"},
        {55, "2900465fcb533e05a158fd2b3be0e5e3b03740d83060aa3580e0d98a96bf2384"},
        {56, "31454ff48ef36af2f08fd511bdc37d9d5855ac23e992e5ff5445cb6b7674a674"},
        {57, "bcc0a5d3791b985b7550e04ca660a6c63a589ba1edd2283c8e110e5b515df124"},
        {63, "5f6401b96532c36de4e65beec0409b69b1d181864c8009b7a04f43e5d56350d1"},
        {64, "94eb5de4943613fd048dc93393ab06877405faa39c11f53e9386083339833e7e"},
        {65, "fc518669b6eb4b4dd91827ecacef86689c725bd5bab888fd3b26dbb196eec954"},
        {127, "0fe729ff19257bd6fec853acc2ea355f6b34b58e6c0f684c3e188fcdfcd9baae"},
        {128, "0aedd4856f8eba0963627336ad5144a9a7dbe12498e6066f0165fc97d8ddee4c"},
        {129, "4f1757ae4bffbae86d775b831765b75af154d52f7deaa46dd378051a2d3ad57f"},
        {1024, "ffbad8f947474cfdd5b2bb22d7e0bf5ee8ba2b7af859d0c2bb28622db6a4be47"},
        {8193, "d37426b71db09954cacd4d2be0059f5dbd7ad5d32fc42ab0eeda4ed1a4e1bebc"},
        {1048576, "3a814d27145f9540c495715da2f85208f3805c323d7da3e9867df2992be4432d"}
    };
    for (const Vector& vector : vectors) {
        std::string bytes(vector.size, '\0');
        for (size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = static_cast<char>((i * 37 + 11) % 256);
        expect(bytes, vector.digest, "Python hashlib binary/padding-boundary oracle");
        const std::string copy = bytes;
        require(identity(copy) == identity(bytes), "same bytes in a new allocation retain identity");
        const std::string offset = "x" + bytes;
        require(rms_jar_identity(reinterpret_cast<const uint8_t*>(offset.data() + 1), bytes.size())
                    == vector.digest, "unaligned input is byte-exact");
        for (size_t position : {size_t(0), bytes.size() / 2, bytes.size() - 1}) {
            std::string changed = bytes;
            changed[position] = static_cast<char>(static_cast<unsigned char>(changed[position]) ^ 1u);
            require(identity(changed) != vector.digest, "one-byte change changes identity");
        }
    }

    // Python oracle for class framing:
    // frame = lambda b: len(b).to_bytes(8, 'big') + b
    // raw = (b'j2me-vm/rms/class-identity/v1\0' + frame(main) +
    //        len(cp).to_bytes(8, 'big') + b''.join(frame(b) for b in cp))
    // hashlib.sha256(raw).hexdigest()
    require(rms_class_identity("Example", {"a", "bc"})
                == "21ba414b44fc00c1752918140676f0bbafbe43fd02af211f4108193047a0d283",
            "Python class framing oracle");
    require(rms_class_identity("Example", {"ab", "c"})
                == "cea3e4c6679ffa1c2fb9126641ded6ddf8c9574691cf5787151133780e0403f4",
            "classpath entries cannot merge ambiguously");
    require(rms_class_identity("Example", {})
                == "2a075f1acae020ec0895c23f5f0d465e728cde3b2977ef5de5b46eebe6e3dde4",
            "Python empty classpath oracle");
    require(rms_class_identity("Example", {""})
                == "551b3e1408bd192ef120fee863399c027cd4851775eb18773e77c871e672e84f",
            "empty classpath differs from one empty entry");
    require(rms_class_identity("ab", {"c"})
                == "30f757c9c58bbc95b97c28f612e7ef31c5a35639a357712af3f300faf42a7221",
            "Python main and classpath boundary oracle");
    require(rms_class_identity("a", {"bc"})
                == "97cc1532fd1b1ababe17a5b32eef5ae673a4228e8714d7d293c4a5285b8f66a8",
            "main and first classpath entry cannot merge");
    require(rms_class_identity("Example", {"a", "bc"}) != rms_class_identity("Example", {"bc", "a"}),
            "classpath order participates in class-launch identity");
    require(rms_class_identity("Example", {"a:b", "c"}) != rms_class_identity("Example", {"a", "b:c"}),
            "classpath delimiter characters are literal bytes");
    require(rms_class_identity("Example", {std::string("a\0b", 3)})
                != rms_class_identity("Example", {"a", "b"}),
            "embedded NUL cannot act as a classpath delimiter");
    require(rms_class_identity(std::string("Example\0suffix", 14), {}) != rms_class_identity("Example", {}),
            "embedded NUL in main class is not truncated");
    require(rms_class_identity("Example", {"a"}) != identity("Examplea"),
            "class launch uses its versioned domain and framing");
    require(rms_class_identity("Example", {"a", "bc"}) == rms_class_identity("Example", {"a", "bc"}),
            "class launch identity is stable");
    std::cout << "ALL SUITE IDENTITY CHECKS COMPLETE " << checks << '\n';
}
