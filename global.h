#pragma once

enum class Kind {
    GET = 2,
    PUT = 3,
    LOCK = 4,
    UNLOCK = 5,
    UPDATE = 6,
};

enum class Status {
    REFUSED = 0,
    SUCCESS = 1
};

const int GET = static_cast<int>(Kind::GET);
const int PUT = static_cast<int>(Kind::PUT);
const int LOCK = static_cast<int>(Kind::LOCK);
const int UNLOCK = static_cast<int>(Kind::UNLOCK);
const int UPDATE = static_cast<int>(Kind::UPDATE);

const int REFUSED = static_cast<int>(Status::REFUSED);
const int SUCCESS = static_cast<int>(Status::SUCCESS);
