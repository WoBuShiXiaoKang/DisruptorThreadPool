#pragma once
#include <atomic>
#include <array>

#define CACHELINE_SIZE_BYTES 64
#define CACHELINE_PADDING_FOR_ATOMIC_INT64_SIZE (CACHELINE_SIZE_BYTES - sizeof(std::atomic_int64_t))
#define CACHELINE_PADDING_FOR_INT64_SIZE (CACHELINE_SIZE_BYTES - sizeof(int64_t))

namespace Kang {

	//��std::atomic_int64_t�����˷�װ���ڴ油�뱣֤_seq��һ����������
	class AtomicSequence
	{
	public:
		AtomicSequence(int64_t num = 0L) : _seq(num) {};
		~AtomicSequence() {};
		AtomicSequence(const AtomicSequence&) = delete;
		AtomicSequence(const AtomicSequence&&) = delete;
		void operator=(const AtomicSequence&) = delete;

		void store(const int64_t val)//, std::memory_order _order = std::memory_order_seq_cst)
		{
			_seq.store(val);//,_order);
		}

		int64_t load()//std::memory_order _order = std::memory_order_seq_cst)
		{
			return _seq.load();// _order);
		}

		int64_t fetch_add(const int64_t increment)//, std::memory_order _order = std::memory_order_seq_cst)
		{
			return _seq.fetch_add(increment);// _order);
		}

	private:
		//���߶����룬�Ա�֤_seq������������������һ��������
		char _frontPadding[CACHELINE_PADDING_FOR_ATOMIC_INT64_SIZE];
		std::atomic_int64_t _seq;
		char _backPadding[CACHELINE_PADDING_FOR_ATOMIC_INT64_SIZE];
	};

	//��int64_t�����˷�װ���ڴ油�뱣֤_seq��һ����������
	class Sequence
	{
	public:
		Sequence(int64_t num = 0L) : _seq(num) {};
		~Sequence() {};
		Sequence(const Sequence&) = delete;
		Sequence(const Sequence&&) = delete;
		void operator=(const Sequence&) = delete;

		void store(const int64_t val)
		{
			_seq = val;
		}

		int64_t load()
		{
			return _seq;
		}

	private:
		//���߶����룬�Ա�֤_seq������������������һ��������
		char _frontPadding[CACHELINE_PADDING_FOR_INT64_SIZE];
		int64_t _seq;
		char _backPadding[CACHELINE_PADDING_FOR_INT64_SIZE];
	};

	//����bufferĬ�ϴ�С,Ϊ�˼�ȡ�����������ĳ�����Ҫ��2��n�η�
	constexpr size_t DefaultRingBufferSize = 262144;

	//��disruptor���ṩ�Ի���buffer�Ĳ���
	//д�ӿڣ�WriteInBuf()
	//���ӿڣ�(1)GetReadableSeq()��ȡ�ɶ��Ļ���buffer��λ�±�
	//        (2)ReadFromBuf()��ȡ�ɶ�������
	//        (3)��������FinishReading()
	//ע�����ӿ�ʹ�ø��ӣ�ʹ����BufConsumer������˷�װ������RAII��
	template<class ValueType, size_t N = DefaultRingBufferSize>
	class Disruptor
	{
	public:
		Disruptor() : _lastRead(-1L) , _lastWrote(-1L), _lastDispatch(-1L), _writableSeq(0L) , _stopWorking(0L){};
		~Disruptor() {};

		Disruptor(const Disruptor&) = delete;
		Disruptor(const Disruptor&&) = delete;
		void operator=(const Disruptor&) = delete;

		static_assert(((N > 0) && ((N& (~N + 1)) == N)),
			"RingBuffer's size must be a positive power of 2");

		//��buffer��д����
		void WriteInBuf(ValueType&& val)
		{
			const int64_t writableSeq = _writableSeq.fetch_add(1);
			while (writableSeq - _lastRead.load() > N)
			{//�ȴ�����
				if (_stopWorking.load())
					throw std::runtime_error("writting when stopped disruptor");
				//std::this_thread::yield();
			}
			//д����
			_ringBuf[writableSeq & (N - 1)] = val;

			while (writableSeq - 1 != _lastWrote.load())
			{//�ȴ�����
			}
			_lastWrote.store(writableSeq);
		};

		//��buffer��д����
		void WriteInBuf(ValueType& val)
		{
			const int64_t writableSeq = _writableSeq.fetch_add(1);
			while (writableSeq - _lastRead.load() > N)
			{//�ȴ�����
				if (_stopWorking.load())
					throw std::runtime_error("writting when stopped disruptor");
				//std::this_thread::yield();
			}
			//д����
			_ringBuf[writableSeq & (N - 1)] = val;

			while (writableSeq - 1 != _lastWrote.load())
			{//�ȴ�����
			}
			_lastWrote.store(writableSeq);
		};

		//��ȡ�ɶ���buffer�±�
		const int64_t GetReadableSeq()
		{
			const int64_t readableSeq = _lastDispatch.fetch_add(1) + 1;
			while (readableSeq > _lastWrote.load())
			{//�ȴ�����
				if (_stopWorking.load() && empty())
				{
					return -1L;
				}
			}
			return readableSeq;
		};

		//��ȡָ���±�λ�õ�buffer����
		ValueType& ReadFromBuf(const int64_t readableSeq)
		{
			if (readableSeq < 0)
			{
				throw("error : incorrect seq for ring Buffer when ReadFromBuf(seq)!");
			}
			return _ringBuf[readableSeq & (N - 1)];
		}

		//��ȡ��ָ���±�λ�õ�buffer���ݣ������±�λ��ʹ��Ȩ
		void FinishReading(const int64_t seq)
		{
			if (seq < 0)
			{
				return;
			}

			while (seq - 1 != _lastRead.load())
			{//�ȴ�����
			}
			//_lastRead = seq;
			_lastRead.store(seq);
		};

		bool empty()
		{
			return _writableSeq.load() - _lastRead.load() == 1;
		}

		//֪ͨdisruptorֹͣ���������øú�������buffer�Ѿ�ȫ�������꣬��ô��ȡ�ɶ��±�ʱֻ���ȡ��-1L
		void stop()
		{
			//_stopWorking = true;
			_stopWorking.store(1L);
		}

	private:
		//���һ���Ѷ�����λ��
		Sequence _lastRead;

		//���һ����д����λ��
		Sequence _lastWrote;

		//disruptor�Ƿ�ֹͣ����
		Sequence _stopWorking;

		//���һ���ɷ���������ʹ�õĲ�λ���
		AtomicSequence _lastDispatch;

		//��ǰ��д�Ĳ�λ���
		AtomicSequence _writableSeq;

		//����buffer��Ϊ�ӿ�ȡ�������N��Ҫʱ2��n����
		std::array<ValueType, N> _ringBuf;
	};

}
