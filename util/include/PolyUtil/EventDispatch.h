#pragma once

#include "PolyUtil/misc.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace wave
{

template<class Handler, typename Id = void*>
class EventDispatch
{
  public:
    EventDispatch() = default;
    ~EventDispatch() = default;
    DELETE_COPY_MOVE(EventDispatch);

    void subscribe(Id id, Handler handler)
    {
        assert(std::ranges::find_if(subscriptions,
                                    [id](const auto& pair)
                                    { return pair.first == id; }) == subscriptions.end());
        subscriptions.emplace_back(std::pair<Id, Handler>{ id, std::move(handler) });
    }

    void unsubscribe(Id id)
    {
        std::erase_if(subscriptions, [id](const auto& pair) { return pair.first == id; });
    }

    template<class... Types>
    void call(Types... args)
    {
        std::ranges::for_each(subscriptions, [args...](auto& pair) { pair.second(args...); });
    }

    template<class... Types>
    void callLast(Types... args)
    {
        if (!subscriptions.empty())
        {
            subscriptions.back().second(args...);
        }
    }

  private:
    std::vector<std::pair<Id, Handler>> subscriptions = {};
};
}
