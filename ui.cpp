#include <iostream>
#include "filesystem.h"

using namespace std;

int main() {
    if (format(5000, "./data.disk") == 0) {
        cout << "format succeeded!" << endl;
    } else {
        cout << "format failed!" << endl;
    }
    block *disk = mount("./data.disk");
    return 0;
}