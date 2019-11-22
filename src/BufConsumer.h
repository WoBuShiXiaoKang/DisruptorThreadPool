#pragma once
#include "Disruptor.h"

namespace Kang {

	//����RAII��װDisruptor�Ķ�������
	//�����ʱ�����GetReadableSeq()��ȡ�ɶ����
	//ʹ��empty()�ж��Ƿ��пɶ�����
	//ʹ��GetContent()��ȡbuffer�е�����
	//������ʱ�����FinishReading()����disruptor��ringbuffer�Ĳ�λʹ��Ȩ
	//
	//ʹ��ʵ����
	//	std::function<void()> task;
	//	{
	//     BufConsumer<std::function<void()>> consumer(this->_tasks);
	//     if (consumer.empty())
	//     {
	//	     return;
	//     }
	//     task = std::move(consumer.GetContent());
	//	}
	//	task();
	template<class ValueType>
	class BufConsumer
	{
	public:

		BufConsumer(Disruptor<ValueType>* disruptor) : _disruptor(disruptor), _seq(-1L) {
			_seq = _disruptor->GetReadableSeq();
		};

		~BufConsumer()
		{
			_disruptor->FinishReading(_seq);
		};

		BufConsumer(const BufConsumer&) = delete;
		BufConsumer(const BufConsumer&&) = delete;
		void operator=(const BufConsumer&) = delete;

		bool empty()
		{
			return _seq < 0;
		}

		ValueType& GetContent()
		{
			return _disruptor->ReadFromBuf(_seq);
		}

	private:
		Disruptor<ValueType>* _disruptor;
		int64_t _seq;
	};

}