/*
    Copyright (C) 2014-2015 Islog

    This file is part of Leosac.

    Leosac is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Leosac is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "core/auth/BaseAuthSource.hpp"
#include <string>

namespace Leosac
{
    namespace Auth
    {
        class WiegandCard;
        using WiegandCardPtr = std::shared_ptr<WiegandCard>;

        /**
        * A wiegand card.
        */
        class WiegandCard : public BaseAuthSource
        {
        public:
            /**
            * Create a WiegandCard object.
            *
            * @param cardid the id of the card in hexadecimal text format
            * @param bits number of bits (wiegand 26, wiegand32 ...)
            */
            WiegandCard(const std::string &cardid, int bits);

            virtual void accept(Tools::IVisitor *visitor) override;

            /**
            * Returns the id of the card, as a hexadecimal string.
            */
            const std::string &card_id() const;

            int nb_bits() const;

            /**
             * Formats a pretty printed string containing information
             * regarding this card.
             */
            virtual std::string to_string() const override;

            /**
             * Returns the integer representation of the
             * card ID.
             *
             * The format (Wiegand 26, 32, ....) is used to build the
             * card number. If no format is recognized, fallback to `to_raw_int()`
             */
            uint64_t to_int() const;

            /**
             * Convert the bits of the card to an integer.
             *
             * This format (Wiegand26, 32, ...) is ignored: all bits are used
             * to build the number.
             */
            uint64_t to_raw_int() const;

        protected:
            /**
             * Extract the card ID, assuming the format to be Wiegand26.
             */
            uint64_t to_wiegand_26() const;

            /**
             * Extract the card ID, assuming the format to be Wiegand34.
             */
            uint64_t to_wiegand_34() const;

            /**
            * Card id
            */
            std::string card_id_;

            /**
            * Number of meaningful bit
            */
            int nb_bits_;
        };
    }
}
