#ifndef HEXICORD_FLAGS_HPP
#define HEXICORD_FLAGS_HPP

#include <utility>          // std::declvar
#include <initializer_list> // std::initializer_list

namespace Hexicord {
    /**
     *  Type-safe wrapper for OR-ed flags.
     *
     *  This class satisfies requirements of BitmaskType concept.
     *
     *  \tparam Flags Usually enum but also can be any type which have 
     *                valid bitwise operators that return Storage.
     *  \tparam Storage Type to use for OR-ed flags store. Following requirement 
     *                  should be met: sizeof(Flag) <= sizeof(Storage)
     */
    template<typename Flag, typename Storage = int>
    class Flags {
    public:
        static_assert((sizeof(Flag) <= sizeof(Storage)), "Flags<> uses Storage type not enough to hold values of Flag.");

        Flags() = default;

        /**
         *  Construct Flags from OR-ed flags.
         * 
         *  \warning No checks performed thus no type safety.
         */
        explicit constexpr inline Flags(Storage flags) noexcept : set_(flags) {}

        /**
         *  Construct Flags with a single flag set.
         */
        constexpr inline Flags(Flag flag) noexcept : set_(flag | 0x00000000) {}

        inline Flags(std::initializer_list<Flag> il) noexcept {
            for (Flag flag : il) {
                set_ |= flag;
            }
        }

        constexpr inline bool get(Flag flag) const noexcept {
            return (set_ & flag) == flag;
        }

        inline void set(Flag flag, bool value = true) noexcept {
            if (value) {
                set_ |= flag;
            } else {
                set_ &= ~flag;
            }
        }

        constexpr inline bool operator!() const noexcept {
            return set_ == 0;
        }

        constexpr inline operator Storage() const noexcept {
            return set_;
        }

        constexpr inline Flags operator~() const noexcept {
            return Flags(~set_);
        }

        constexpr inline Flags operator&(Flag flag) const noexcept {
            return Flags(set_ & flag);
        }

        constexpr inline Flags operator|(Flag flag) const noexcept {
            return Flags(set_ | flag);
        }

        constexpr inline Flags operator^(Flag flag) const noexcept {
            return Flags(set_ ^ flag);
        }

        constexpr inline Flags operator&(const Flags& flags) const noexcept {
            return Flags(set_ & flags.set_);
        }

        constexpr inline Flags operator|(const Flags& flags) const noexcept {
            return Flags(set_ | flags.set_);
        }
        
        constexpr inline Flags operator^(const Flags& flags) const noexcept {
            return Flags(set_ ^ flags.set_);
        }

        Flags& operator&=(Flag flag) noexcept {
            set_ &= flag;
            return *this;
        }

        Flags& operator|=(Flag flag) noexcept {
            set_ |= flag;
            return *this;
        }

        Flags& operator^=(Flag flag) noexcept {
            set_ ^= flag;
            return *this;
        }

        Flags& operator&=(const Flags& flags) noexcept {
            set_ &= flags.set_;
            return *this;
        }

        Flags& operator|=(const Flags& flags) noexcept {
            set_ |= flags.set_;
            return *this;
        }

        Flags& operator^=(const Flags& flags) noexcept {
            set_ ^= flags.set_;
            return *this;
        }
    private:
        Storage set_ = 0x00000000;
    };

#define DECLARE_FLAGS(flagtype, name) \
        using name = Flags<flagtype>

#define DECLARE_FLAGS_OPERATORS(flagtype) \
        constexpr inline Flags<flagtype> operator|(flagtype lhs, flagtype rhs) noexcept { \
            return Flags<flagtype>(lhs | rhs); \
        } \
        constexpr inline Flags<flagtype> operator|(flagtype rhs, Flags<flagtype> lhs) noexcept { \
            return Flags<flagtype>(rhs | lhs); \
        }

} // namespace Hexicord

#endif // HEXICORD_FLAGS_HPP

