/*
 * oop.hpp
 *
 *  Created on: 2017年11月10日
 *      Author: zhengxiaolin
 */

#ifndef INCLUDE_RUNTIME_OOP_HPP_
#define INCLUDE_RUNTIME_OOP_HPP_

#include "runtime/klass.hpp"
#include "runtime/field.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>

template <typename Tp, typename Arg1>
void __constructor(Tp *ptr, const Arg1 & arg1)		// 适配一个参数
{
	::new ((void *)ptr) Tp(arg1);
}

template <typename Tp, typename Arg1, typename Arg2>
void __constructor(Tp *ptr, const Arg1 & arg1, const Arg2 & arg2)		// 适配两个参数
{
	::new ((void *)ptr) Tp(arg1, arg2);
}

template <typename Tp, typename Arg1, typename Arg2, typename Arg3>
void __constructor(Tp *ptr, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3)		// 适配三个参数
{
	::new ((void *)ptr) Tp(arg1, arg2, arg3);
}

template <typename Tp, typename ...Args>
void constructor(Tp *ptr, Args &&...args)		// placement new.
{
	__constructor(ptr, std::forward<Args>(args)...);		// 完美转发变长参数
}

template <typename Tp>
void destructor(Tp *ptr)
{
	ptr->~Tp();
}

class Mempool {		// TODO: 此类必须实例化！！内存池 Heap！！适用于多线程！因此 MemAlloc 应该内含一个实例化的 Mempool 对象才行！

};

class MemAlloc {
public:
	static void *allocate(size_t size) {		// TODO: change to real Mem Pool (Heap)
		void *ptr = malloc(size);
		memset(ptr, 0, size);		// default bzero!
		return ptr;
	}
	static void deallocate(void *ptr) { free(ptr); }
	void *operator new(size_t size) throw() { return allocate(size); }
	void *operator new(size_t size, const std::nothrow_t &) throw() { return allocate(size); }
	void *operator new[](size_t size) throw() { return allocate(size); }
	void *operator new[](size_t size, const std::nothrow_t &) throw() { return allocate(size); }
	void operator delete(void *ptr) { return deallocate(ptr); }
	void operator delete[](void *ptr) { return deallocate(ptr); }
};


class Oop : public MemAlloc {		// 注意：Oop 必须只能使用 new ( = ::operator new + constructor-->::operator new(pointer p) )来分配！！因为要放在堆中。
protected:
	// TODO: HashCode .etc
	OopType ooptype;
	shared_ptr<Klass> klass;
public:
	// TODO: HashCode .etc
	shared_ptr<Klass> get_klass() { return klass; }
public:
	explicit Oop(shared_ptr<Klass> klass, OopType ooptype) : klass(klass), ooptype(ooptype) {}
	OopType get_ooptype() { return ooptype; }
};

class InstanceOop : public Oop {	// Oop::klass must be an InstanceKlass type.
private:
	int field_length;
	uint8_t *fields = nullptr;	// save a lot of mixed datas. int, float, Long, Reference... if it's Reference, it will point to a Oop object.
public:
	InstanceOop(shared_ptr<InstanceKlass> klass);
public:		// 以下 8 个方法全部用来赋值。
	bool get_field_value(shared_ptr<Field_info> field, uint64_t *result);
	void set_field_value(shared_ptr<Field_info> field, uint64_t value);
	bool get_field_value(const wstring & signature, uint64_t *result);				// use for forging String Oop at parsing constant_pool.
	void set_field_value(const wstring & signature, uint64_t value);	// same as above...
	bool get_static_field_value(shared_ptr<Field_info> field, uint64_t *result) { return std::static_pointer_cast<InstanceKlass>(klass)->get_static_field_value(field, result); }
	void set_static_field_value(shared_ptr<Field_info> field, uint64_t value) { std::static_pointer_cast<InstanceKlass>(klass)->set_static_field_value(field, value); }
	bool get_static_field_value(const wstring & signature, uint64_t *result) { return std::static_pointer_cast<InstanceKlass>(klass)->get_static_field_value(signature, result); }
	void set_static_field_value(const wstring & signature, uint64_t value) { std::static_pointer_cast<InstanceKlass>(klass)->set_static_field_value(signature, value); }
//public:	// deprecated.
//	unsigned long get_value(const wstring & signature);
//	void set_value(const wstring & signature, unsigned long value);
};

class MirrorOop : public InstanceOop {	// for java_mirror. Because java_mirror->klass must be java.lang.Class...... We'd add a varible: mirrored_who.
private:
	shared_ptr<Klass> mirrored_who;		// this Oop being instantiation, must after java.lang.Class loaded !!!
public:
	MirrorOop(shared_ptr<Klass> mirrored_who);
public:
	shared_ptr<Klass> get_mirrored_who() { return mirrored_who; }
};

class ArrayOop : public Oop {
protected:
	int length;
	Oop **buf = nullptr;		// 注意：这是一个指针数组！！内部全部是指针！这样设计是为了保证 ArrayOop 内部可以嵌套 ArrayOop 的情况，而且也非常符合 Java 自身的特点。
public:
	ArrayOop(shared_ptr<ArrayKlass> klass, int length, OopType ooptype) : Oop(klass, ooptype), length(length), buf((Oop **)MemAlloc::allocate(sizeof(Oop *) * length)) {}	// **only malloc (sizeof(ptr) * length) !!!!**
	int get_length() { return length; }
	int get_dimension() { return std::static_pointer_cast<ArrayKlass>(klass)->get_dimension(); }
	Oop* & operator[] (int index) {
		assert(index >= 0 && index < length);	// TODO: please replace with ArrayIndexOutofBound...
		return buf[index];
	}
	const Oop* operator[] (int index) const {
		return this->operator[](index);
	}
	~ArrayOop() {
		if (buf != nullptr) {
			for (int i = 0; i < length; i ++) {
				MemAlloc::deallocate(buf[i]);
			}
			MemAlloc::deallocate(buf);
		}
	}
};

class TypeArrayOop : public ArrayOop {
public:		// Most inner type of `buf` is BasicTypeOop.
	TypeArrayOop(shared_ptr<TypeArrayKlass> klass, int length) : ArrayOop(klass, length, OopType::_TypeArrayOop) {}
public:

};

class ObjArrayOop : public ArrayOop {
public:		// Most inner type of `buf` is InstanceOop.		// 注意：维度要由自己负责！！并不会进行检查。
	ObjArrayOop(shared_ptr<ObjArrayKlass> klass, int length) : ArrayOop(klass, length, OopType::_ObjArrayOop) {}
public:
};


// Basic Type...

class BasicTypeOop : public Oop {
private:
	Type type;	// only allow for BYTE, BOOLEAN, CHAR, SHORT, INT, FLOAT, LONG, DOUBLE.
public:
	BasicTypeOop(Type type) : Oop(nullptr, OopType::_BasicTypeOop), type(type) {}
	Type get_type() { return type; }
};

struct ByteOop : public BasicTypeOop {
	uint8_t value;		// data
	ByteOop(uint8_t value) : BasicTypeOop(Type::BYTE), value(value) {}
};

struct BooleanOop : public BasicTypeOop {
	uint8_t value;		// data
	BooleanOop(uint8_t value) : BasicTypeOop(Type::BOOLEAN), value(value) {}
};

struct CharOop : public BasicTypeOop {
	uint32_t value;		// data		// [x] must be 16 bits!! for unicode (jchar is unsigned short)	// I modified it to 32 bits... Though of no use, jchar of 16 bits is very bad design......
	CharOop(uint32_t value) : BasicTypeOop(Type::CHAR), value(value) {}
};

struct ShortOop : public BasicTypeOop {
	uint16_t value;		// data
	ShortOop(uint16_t value) : BasicTypeOop(Type::SHORT), value(value) {}
};

struct IntOop : public BasicTypeOop {
	uint32_t value;		// data
	IntOop(uint32_t value) : BasicTypeOop(Type::INT), value(value) {}
};

struct FloatOop : public BasicTypeOop {
	float value;		// data
	FloatOop(uint32_t value) : BasicTypeOop(Type::FLOAT), value(value) {}
};

struct LongOop : public BasicTypeOop {
	uint64_t value;		// data
	LongOop(uint64_t value) : BasicTypeOop(Type::LONG), value(value) {}
};

struct DoubleOop : public BasicTypeOop {
	double value;		// data
	DoubleOop(uint64_t value) : BasicTypeOop(Type::DOUBLE), value(value) {}
};

#endif /* INCLUDE_RUNTIME_OOP_HPP_ */
