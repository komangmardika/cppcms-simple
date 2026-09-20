#ifndef __NULLABLE_H__
#define __NULLABLE_H__

// A value that may be absent.
//
// The project builds as C++11, so std::optional is not available. This covers
// the one thing the person table needs: telling "no address at all" apart from
// "an address that happens to be the empty string".
template <typename T>
class Nullable
{
public:
    Nullable() : value_(), has_value_(false) {}
    Nullable(const T &value) : value_(value), has_value_(true) {}

    bool hasValue() const { return has_value_; }

    // Only meaningful when hasValue() is true; returns a default-constructed
    // T otherwise so callers cannot read uninitialised memory.
    const T &value() const { return value_; }

    T valueOr(const T &fallback) const { return has_value_ ? value_ : fallback; }

    void set(const T &value)
    {
        value_ = value;
        has_value_ = true;
    }

    void clear()
    {
        value_ = T();
        has_value_ = false;
    }

private:
    T value_;
    bool has_value_;
};

#endif // __NULLABLE_H__
