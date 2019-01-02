//
// Created by philipp on 02.01.19.
//

#ifndef ASIONET_WORKSERIALIZER_H
#define ASIONET_WORKSERIALIZER_H

#include <boost/asio.hpp>
#include "Context.h"

namespace asionet
{

class WorkSerializer : public asionet::Context::strand
{
public:
	explicit WorkSerializer(asionet::Context & context)
		: asionet::Context::strand(context)
	{}

	template<typename Handler>
	boost::asio::executor_binder<typename boost::asio::decay<Handler>::type, typename asionet::Context::strand>
	operator()(Handler && handler)
	{
		return boost::asio::bind_executor(*this, handler);
	}
};

}

#endif //ASIONET_WORKSERIALIZER_H
