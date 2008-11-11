/*
 * This file is part of KDevelop
 *
 * Copyright 2008 David Nolden <david.nolden.kdevelop@art-master.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <QtGui/qevent.h>
#include <qdatetime.h>


///A helper-class to detect shift-presses without anything else pressed.
///Just feed in the eventsa, and look at he return-value.
struct ShiftPressDetector {
    public:
        ///@param secure If this is true, the detector will only trigger of no other key was pressed in 500 ms before the shift key.
        ///This is useful to prevent mis-triggering.
        inline ShiftPressDetector(bool secure = false) : m_hadOtherKey(true), m_lastOtherKey(QTime::currentTime()), m_secure(secure) {
        }
        ///Must be called with all key-events
        ///Returns true if the shift-key was released and no other key was pressed between its press and release.
        inline bool checkKeyEvent(QKeyEvent* e) {
            
            if(e->key() != Qt::Key_Shift)
                m_lastOtherKey = QTime::currentTime();
            
            if(e->type() == QEvent::KeyPress) {
                m_hadOtherKey = true;
                if (e->key() == Qt::Key_Shift && (e->modifiers() & (~Qt::ShiftModifier)) == 0) {
                    if(m_secure && m_lastOtherKey.msecsTo(QTime::currentTime()) > 500)
                        m_hadOtherKey = false;
                }
                
            }else if(e->type() == QEvent::KeyRelease) {
                if(e->key() == Qt::Key_Shift && !m_hadOtherKey)
                    return true;
            }
            
            return false;
        }
        
        ///Call this to reset the detector
        inline void clear() {
            m_hadOtherKey = true;
        }
    private:
    bool m_hadOtherKey;
    QTime m_lastOtherKey;
    bool m_secure;
};