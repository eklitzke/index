#include "./file_util.h"

#include "./context.h"
#include "./util.h"

#include <cassert>
#include <set>
#include <vector>

namespace {
std::map<std::string, std::string> file_types_;

std::vector<std::string> bad_dirs_;
std::set<std::string> bad_exts_;

struct Init_ {
  Init_() {
    file_types_.insert(std::make_pair("C", "c++"));
    file_types_.insert(std::make_pair("LICENSE", "text"));
    file_types_.insert(std::make_pair("Makefile", "make"));
    file_types_.insert(std::make_pair("NOTICE", "text"));
    file_types_.insert(std::make_pair("README", "text"));
    file_types_.insert(std::make_pair("S", "asm"));
    file_types_.insert(std::make_pair("ada", "ada"));
    file_types_.insert(std::make_pair("am", "make"));
    file_types_.insert(std::make_pair("asm", "asm"));
    file_types_.insert(std::make_pair("bash", "shell"));
    file_types_.insert(std::make_pair("bat", "shell"));
    file_types_.insert(std::make_pair("c", "c"));
    file_types_.insert(std::make_pair("cc", "c++"));
    file_types_.insert(std::make_pair("clj", "clojure"));
    file_types_.insert(std::make_pair("conf", "text"));
    file_types_.insert(std::make_pair("cpp", "c++"));
    file_types_.insert(std::make_pair("cs", "csharp"));
    file_types_.insert(std::make_pair("css", "css"));
    file_types_.insert(std::make_pair("el", "elisp"));
    file_types_.insert(std::make_pair("gitignore", "text"));
    file_types_.insert(std::make_pair("go", "go"));
    file_types_.insert(std::make_pair("h", "c"));
    file_types_.insert(std::make_pair("hh", "c++"));
    file_types_.insert(std::make_pair("hpp", "c++"));
    file_types_.insert(std::make_pair("hs", "haskell"));
    file_types_.insert(std::make_pair("htm", "html"));
    file_types_.insert(std::make_pair("html", "html"));
    file_types_.insert(std::make_pair("in", "make"));
    file_types_.insert(std::make_pair("ipp", "c++"));
    file_types_.insert(std::make_pair("java", "java"));
    file_types_.insert(std::make_pair("js", "javascript"));
    file_types_.insert(std::make_pair("json", "javascript"));
    file_types_.insert(std::make_pair("lua", "lua"));
    file_types_.insert(std::make_pair("m", "obj-c"));
    file_types_.insert(std::make_pair("m4", "m4"));
    file_types_.insert(std::make_pair("man", "man"));
    file_types_.insert(std::make_pair("md", "text"));
    file_types_.insert(std::make_pair("mk", "make"));
    file_types_.insert(std::make_pair("ml", "ml"));
    file_types_.insert(std::make_pair("mm", "obj-c"));
    file_types_.insert(std::make_pair("php", "php"));
    file_types_.insert(std::make_pair("pl", "perl"));
    file_types_.insert(std::make_pair("pm", "perl"));
    file_types_.insert(std::make_pair("proto", "protobuf"));
    file_types_.insert(std::make_pair("py", "python"));
    file_types_.insert(std::make_pair("rb", "ruby"));
    file_types_.insert(std::make_pair("s", "asm"));
    file_types_.insert(std::make_pair("sh", "shell"));
    file_types_.insert(std::make_pair("sql", "sql"));
    file_types_.insert(std::make_pair("tex", "tex"));
    file_types_.insert(std::make_pair("txt", "text"));
    file_types_.insert(std::make_pair("vb", "vb"));
    file_types_.insert(std::make_pair("vbs", "vb"));
    file_types_.insert(std::make_pair("xhtml", "html"));
    file_types_.insert(std::make_pair("xml", "xml"));
    file_types_.insert(std::make_pair("xsd", "xml"));
    file_types_.insert(std::make_pair("yaml", "yaml"));
    file_types_.insert(std::make_pair("yml", "yaml"));

    // If any part of the file path contains one of these directory
    // names, we will not index the file.
    bad_dirs_.push_back(".git");
    bad_dirs_.push_back(".hg");
    bad_dirs_.push_back(".repo");
    bad_dirs_.push_back(".svn");

    // These are files that we never, ever want to index. Some of
    // these are here just because we know they're always binary and
    // it doesn't make sense to read a bunch of data and apply our
    // UTF-8 heuristics; others, like pdf, are here because the
    // heuristics sometimes mark them as text even though they are
    // binary.
    bad_exts_.insert("a");
    bad_exts_.insert("jpg");
    bad_exts_.insert("jpeg");
    bad_exts_.insert("mo");
    bad_exts_.insert("o");
    bad_exts_.insert("pdf");  // looks like text sometimes
    bad_exts_.insert("png");
    bad_exts_.insert("so");
    bad_exts_.insert("swp");
  }
};

// initialize the compilation unit
Init_ init_;

// Get the extension of a file. For instance,
//  foo.png   -> png
//  Makefile  -> Makefile
std::string GetExtension(const std::string &filename) {
  std::string::size_type dotpos = filename.find_last_of('.');
  if (dotpos == std::string::npos) {
    dotpos = 0;
  } else {
    dotpos++;
  }
  return filename.substr(dotpos, std::string::npos);
}
}

namespace codesearch {
std::string FileLanguage(const std::string &filename) {
  std::string extension = GetExtension(filename);
  const auto &it = file_types_.find(extension);
  if (it == file_types_.end()) {
    return "";
  } else {
    return it->second;
  }
}

bool ShouldIndex(const std::string &filename, std::size_t read_size) {
  // Try to detect files in directories like .git, .hg, etc.
  for (const auto &dirname : bad_dirs_) {
    if (filename.find("/" + dirname + "/") != std::string::npos) {
      return false;
    }
  }

  // Detect bad file extensions, like .pdf
  std::string extension = GetExtension(filename);
  if (bad_exts_.find(extension) != bad_exts_.end()) {
    return false;
  }

  // We're going to read the first 10kish bytes of the file (breaking
  // on newline boundaries), and detect what % of the data looks like
  // it's UTF-8; if we get 95% or more valid UTF-8 data, then we
  // choose to index the file.
  std::ifstream ifs(filename, std::ifstream::in | std::ifstream::binary);
  assert(!ifs.fail());
  std::string s;
  std::size_t valid_data = 0;
  std::size_t invalid_data = 0;
  while (true) {
    std::getline(ifs, s);
    if (IsValidUtf8(s)) {
      valid_data += s.size();
    } else {
      invalid_data += s.size();
    }
    if (ifs.eof() || ifs.tellg() == -1) {
      break;
    }
    if (static_cast<std::size_t>(ifs.tellg()) >= read_size) {
      break;
    }
    assert(!ifs.fail());
    s.clear();  // is this necessary?
  }
  return valid_data > 0 && valid_data >= 20 * invalid_data;
}

std::map<std::size_t, std::string> GetFileContext(std::ifstream *ifs,
                                                  std::size_t line_number,
                                                  std::uint64_t offset,
                                                  std::size_t context) {
  std::size_t p = offset;
  std::size_t lines_found_reverse = 0;

  // N.B. This is really inefficient, because we are re-seeking for
  // each character encountered. The portable/fastish way to do it is
  // do have some buffering here (e.g. read 100 characters at a time
  // backwards), the unportable/really-fast way to do it is to use
  // mmap(2) with memrchr(3) to search backwards for newlines.
  while (p) {
    p--;
    ifs->seekg(p);
    assert(!ifs->fail() && !ifs->eof());
    if (ifs->peek() == '\n') {
      if (lines_found_reverse == context) {
        p++;
        ifs->seekg(p);
        assert(!ifs->fail() && !ifs->eof());
        break;
      } else {
        lines_found_reverse++;
      }
    }
  }

  // The file is aligned to where we should start reading data, and we
  // can figure out how many lines we are back based on
  // lines_found_reverse.
  assert(lines_found_reverse <= line_number);
  std::map<std::size_t, std::string> line_map;
  for (std::size_t line = line_number - lines_found_reverse;
       line < line_number + context + 1; line++) {
    std::string line_str;
    getline(*ifs, line_str);
    line_map.insert(std::make_pair(line, line_str));
  }
  return line_map;
}

// Same, but automatically opens the file for reading.
std::map<std::size_t, std::string> GetFileContext(const std::string &file_path,
                                                  std::size_t line_number,
                                                  std::uint64_t offset,
                                                  std::size_t context) {
  std::string fp = file_path;
  if (file_path.empty() || *file_path.begin() != '/') {
    Context *ctx = GetContext();
    fp = ctx->vestibule() + "/" + file_path;
  }

  std::ifstream ifs(fp, std::ifstream::binary | std::ifstream::in);
  assert(!ifs.fail() && !ifs.eof());
  return GetFileContext(&ifs, line_number, offset, context);
}
}
