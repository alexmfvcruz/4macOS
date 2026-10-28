#include "../role_workers.hpp"
#include <cassert>
#include <future>
#include <iostream>
using namespace fourstory::server;
int main() {
    Config config;config.service=Service::map;config.queue_limit=2;
    RoleWorkers workers(config);assert(workers.has(Lane::batch) && workers.has(Lane::database));
    assert(workers.has(Lane::ai) && workers.has(Lane::log) && !workers.has(Lane::management));
    std::promise<void> entered,release,db_finished;auto released=release.get_future().share();
    std::vector<unsigned> order;
    assert(workers.submit(Lane::batch,[&]{entered.set_value();released.wait();order.push_back(1);}));
    entered.get_future().wait();
    assert(workers.submit(Lane::batch,[&]{order.push_back(2);}));
    assert(workers.submit(Lane::batch,[&]{order.push_back(3);}));
    assert(!workers.submit(Lane::batch,[]{}));
    assert(workers.submit(Lane::database,[&]{db_finished.set_value();}));
    assert(db_finished.get_future().wait_for(std::chrono::seconds(2))==std::future_status::ready);
    release.set_value();workers.stop();assert(order==std::vector<unsigned>({1,2,3}));
    assert(workers.idle() && !workers.submit(Lane::log,[]{}));
    config.service=Service::control;RoleWorkers control(config);
    assert(control.has(Lane::management) && control.has(Lane::timer) && !control.has(Lane::database));
    config.service=Service::world;RoleWorkers world(config);
    assert(world.has(Lane::database) && world.has(Lane::timer) && !world.has(Lane::ai));
    config.service=Service::login;RoleWorkers login(config);
    assert(login.has(Lane::network) && !login.has(Lane::batch));
    std::cout<<"PASS original role lane layouts, independent DB work, FIFO batch order, saturation and drain\n";
}
