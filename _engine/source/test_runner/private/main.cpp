#include "test/test_framework.h"

int main() {
    return Entelechy::Test::TestRegistry::instance().runAll();
}
