#pragma once

/****************************************************************************************
 ** GitQlient is an application to manage and operate one or several Git repositories. With
 ** GitQlient you will be able to add commits, branches and manage all the options Git provides.
 ** Copyright (C) 2020  Francesc Martinez
 **
 ** LinkedIn: www.linkedin.com/in/cescmm/
 ** Web: www.francescmm.com
 **
 ** This library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public
 ** License as published by the Free Software Foundation; either
 ** version 2 of the License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this library; if not, write to the Free Software
 ** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***************************************************************************************/

#include <QFrame>

class QSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;

/*!
 \brief The GeneralConfigPage shows the available configuration for GitQlient. The configurable options are the
following:
- Auto-fetch: The user can configure the interval where the auto-fetch runs. It performs a git fetch.
- Auto-prune: The user can configure the interval where GitQlient performs a prune.
- Disable logs: The user can enable or disable logs.
- Log level: The user can configure the level of the logs for GitQlient.

*/
class GeneralConfigPage : public QFrame
{
   Q_OBJECT

public:
   /*!
    \brief Default constructor.

    \param parent The parent widget if needed.
   */
   explicit GeneralConfigPage(QWidget *parent = nullptr);

private:
   QSpinBox *mAutoFetch = nullptr;
   QCheckBox *mAutoPrune = nullptr;
   QCheckBox *mDisableLogs = nullptr;
   QComboBox *mLevelCombo = nullptr;
   QCheckBox *mAutoFormat = nullptr;
   QLabel *mStatusLabel = nullptr;
   QLineEdit *mExternalEditor = nullptr;
   QComboBox *mStylesSchema = nullptr;
   QPushButton *mReset = nullptr;
   QPushButton *mApply = nullptr;

   /*!
    \brief Reset the changes to its default value.

   */
   void resetChanges();
   /*!
    \brief Applies the changes into the settings system.

   */
   void applyChanges();
};
