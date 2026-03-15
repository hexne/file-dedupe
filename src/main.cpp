import std;
import text;
import directory;

int main() {

    std::string dir = "/mnt/c/Users/hexne/Desktop/小说";
    auto text_class_info = analyse_text(dir);
    class_to(text_class_info, dir + "_tmp");

    return 0;
}