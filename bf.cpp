#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <libtcc.h>

void bfc(const std::string& bf_code) {
  std::stringstream os;
  os << "#include <stdio.h>\n"
     << "int bf (){\n"
     << "char a[30000] = {0};\n"
     << "char *p = a;\n";
  for(auto c : bf_code) {
    switch(c) {
      case '>': os << "++p;"; break;
      case '<': os << "--p;"; break;
      case '+': os << "++*p;"; break;
      case '-': os << "--*p;"; break;
      case '.': os << "putchar(*p);"; break;
      case ',': os << "*p=getchar();"; break;
      case '[': os << "while(*p){"; break;
      case ']': os << "}"; break;
    }
  }
  os << "return 0;\n"
     << "}\n";
  auto c_code = os.str();
  auto *s = tcc_new();
  tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
  tcc_compile_string(s, c_code.c_str());
  tcc_relocate(s, TCC_RELOCATE_AUTO);
  auto bf = (int (*)())tcc_get_symbol(s, "bf");
  bf();
  tcc_delete(s);
}

void bfi(const std::string& bf_code) {
  char a[30000] = {0};
  char *p = a;
  int i = 0;
  while(i < bf_code.size()) {
    switch(bf_code[i]) {
      case '>': ++p; break;
      case '<': --p; break;
      case '+': ++*p; break;
      case '-': --*p; break;
      case '.': putchar(*p); break;
      case ',': *p=getchar(); break;
      case '[': {
        if(*p == 0) {
          int cnt = 1;
          while(cnt > 0) {
            ++i;
            if(bf_code[i] == '[') ++cnt;
            if(bf_code[i] == ']') --cnt;
          }
        }
        break;
      }
      case ']': {
        if(*p != 0) {
          int cnt = 1;
          while(cnt > 0) {
            --i;
            if(bf_code[i] == '[') --cnt;
            if(bf_code[i] == ']') ++cnt;
          }
        }
        break;
      }
    }
    ++i;
  }
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    auto filename = argv[1];
    std::ifstream ifs(filename);
    std::string bf_code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    bfi(bf_code);
  } else if (argc == 3 && argv[1] == std::string("--jit")) {
    auto filename = argv[2];
    std::ifstream ifs(filename);
    std::string bf_code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    bfc(bf_code);
  } else {
    std::cerr << "usage: bf [--jit] <filename>" << std::endl;
    std::cerr << "your input: " << "bf ";
    for(int i = 1; i < argc; ++i) {
      std::cerr << argv[i] << " ";
    }
    return 1;
  }
  return 0;
}
