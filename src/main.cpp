#include "core/logger.h"

int main() {
    logger_init();
    log_info("hi friend");
    logger_quit();
    return 0;
}
