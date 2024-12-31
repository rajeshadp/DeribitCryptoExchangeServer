//
// Created by root1 on 20/10/24.
//

#ifndef OBSERVER_H
#define OBSERVER_H

#include <iostream>

class Observer {
    public:
        virtual void on_token_updated(const std::string& new_token) = 0; // Observer interface
    };



#endif //OBSERVER_H
