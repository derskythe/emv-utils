/**
 * @file emv-viewer-mainwindow.cpp
 * @brief Main window of EMV Viewer
 *
 * Copyright 2024 Leon Lynch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "emv-viewer-mainwindow.h"
#include "emvhighlighter.h"
#include "emvtreeview.h"
#include "emvtreeitem.h"

#include <QtCore/QStringLiteral>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtGui/QDesktopServices>

#include <cctype>

EmvViewerMainWindow::EmvViewerMainWindow(
	QWidget* parent,
	QString overrideData,
	int overrideDecodeCheckBoxState
)
: QMainWindow(parent)
{
	// Prepare timer used to bundle tree view updates. Do this before setupUi()
	// calls QMetaObject::connectSlotsByName() to ensure that auto-connect
	// works for on_updateTimer_timeout().
	updateTimer = new QTimer(this);
	updateTimer->setObjectName(QStringLiteral("updateTimer"));
	updateTimer->setSingleShot(true);

	// Setup UI widgets
	setupUi(this);
	setWindowIcon(QIcon(":icons/openemv_emv_utils_512x512.png"));
	setWindowTitle(windowTitle().append(QStringLiteral(" (") + qApp->applicationVersion() + QStringLiteral(")")));

	// Note that EmvHighlighter assumes that all blocks are processed in order
	// for every change to the text. Therefore rehighlight() must be called
	// whenever the widget text changes. See on_dataEdit_textChanged().
	highlighter = new EmvHighlighter(dataEdit->document());

	// Set initial state of checkboxes for highlighter and tree view because
	// checkboxes will only emit a stateChanged signal if loadSettings()
	// changes the value to be different from the initial state.
	highlighter->setEmphasiseTags(tagsCheckBox->isChecked());
	highlighter->setIgnorePadding(paddingCheckBox->isChecked());
	treeView->setIgnorePadding(paddingCheckBox->isChecked());
	treeView->setDecodeFields(decodeCheckBox->isChecked());

	// Load previous UI values
	loadSettings();

	// Load values from command line options
	if (!overrideData.isEmpty()) {
		dataEdit->setPlainText(overrideData);
	}
	if (overrideDecodeCheckBoxState > -1) {
		decodeCheckBox->setCheckState(static_cast<Qt::CheckState>(overrideDecodeCheckBoxState));
	}

	// Default to showing legal text in description widget
	displayLegal();
}

void EmvViewerMainWindow::closeEvent(QCloseEvent* event)
{
	// Save current UI values
	saveSettings();
}

void EmvViewerMainWindow::loadSettings()
{
	QSettings settings;
	QList<QCheckBox*> check_box_list = findChildren<QCheckBox*>();

	settings.beginGroup(QStringLiteral("settings"));

	// Iterate over checkboxes and load from settings
	for (auto check_box : check_box_list) {
		Qt::CheckState state;

		if (!settings.contains(check_box->objectName())) {
			// No value to load
			continue;
		}

		state = static_cast<Qt::CheckState>(settings.value(check_box->objectName()).toUInt());
		check_box->setCheckState(state);
	}

	// Load window and splitter states from settings
	restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
	splitter->restoreState(settings.value(QStringLiteral("splitterState")).toByteArray());
	if (settings.contains(QStringLiteral("splitterBottomState"))) {
		splitterBottom->restoreState(settings.value(QStringLiteral("splitterBottomState")).toByteArray());
	} else {
		// Favour tree view child if no saved state available
		splitterBottom->setSizes({99999, 1});
	}
}

void EmvViewerMainWindow::saveSettings() const
{
	QSettings settings;
	QList<QCheckBox*> check_box_list = findChildren<QCheckBox*>();

	// Start with blank settings
	settings.clear();
	settings.beginGroup(QStringLiteral("settings"));

	// Iterate over checkboxes and save to settings
	for (auto check_box : check_box_list) {
		if (!check_box->isChecked()) {
			// Don't save unchecked checkboxes
			continue;
		}

		settings.setValue(check_box->objectName(), check_box->checkState());
	}

	// Save window and splitter states
	settings.setValue(QStringLiteral("geometry"), saveGeometry());
	settings.setValue(QStringLiteral("splitterState"), splitter->saveState());
	settings.setValue(QStringLiteral("splitterBottomState"), splitterBottom->saveState());


	settings.sync();
}

void EmvViewerMainWindow::displayLegal()
{
	// Display copyright, license and disclaimer notice
	descriptionText->clear();
	descriptionText->appendHtml(QStringLiteral(
		"Copyright 2021-2024 <a href='https://github.com/leonlynch'>Leon Lynch</a><br/><br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 3 as published by the Free Software Foundation.<br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.<br/>"
		"See <a href='https://raw.githubusercontent.com/openemv/emv-utils/master/viewer/LICENSE.gpl'>LICENSE.gpl</a> file for more details.<br/><br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> uses various libraries including:<br/>"
		"- <a href='https://github.com/openemv/emv-utils'>emv-utils</a> (licensed under <a href='https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html'>LGPL v2.1</a>)<br/>"
		"- <a href='https://www.qt.io'>Qt</a> (licensed under <a href='https://www.gnu.org/licenses/lgpl-3.0.html'>LGPL v3</a>)<br/>"
		"<br/>"
		"EMV\xAE is a registered trademark in the U.S. and other countries and an unregistered trademark elsewhere. The EMV trademark is owned by EMVCo, LLC. "
		"This program refers to \"EMV\" only to indicate the specifications involved and does not imply any affiliation, endorsement or sponsorship by EMVCo in any way."
	));

	// Let description scroll to top after updating content
	QTimer::singleShot(0, [this]() {
		descriptionText->verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
	});
}

void EmvViewerMainWindow::parseData()
{
	QString str;
	int validLen;
	QByteArray data;
	unsigned int validBytes;

	str = dataEdit->toPlainText();
	if (str.isEmpty()) {
		treeView->clear();
		return;
	}

	// Remove all whitespace from hex string
	str = str.simplified().remove(' ');
	validLen = str.length();

	// Ensure that hex string contains only hex digits
	for (int i = 0; i < validLen; ++i) {
		if (!std::isxdigit(str[i].unicode())) {
			// Only parse up to invalid digit
			validLen = i;
			break;
		}
	}

	// Ensure that hex string has even number of digits
	if (validLen & 0x01) {
		// Odd number of digits. Ignore last digit to see whether parsing can
		// proceed regardless and indicate invalid data later.
		validLen -= 1;
	}

	data = QByteArray::fromHex(str.left(validLen).toUtf8());
	validBytes = treeView->populateItems(data);
	validLen = validBytes * 2;

	if (validLen < str.length()) {
		// Remaining data is invalid and unlikely to be padding
		QTreeWidgetItem* item = new QTreeWidgetItem(
			treeView->invisibleRootItem(),
			QStringList(
				QStringLiteral("Remaining invalid data: ") +
				str.right(str.length() - validLen)
			)
		);
		item->setDisabled(true);
		item->setForeground(0, Qt::red);
	}
}

void EmvViewerMainWindow::on_updateTimer_timeout()
{
	parseData();
}

void EmvViewerMainWindow::on_dataEdit_textChanged()
{
	// Rehighlight when text changes. This is required because EmvHighlighter
	// assumes that all blocks are processed in order for every change to the
	// text. Note that rehighlight() will also re-trigger the textChanged()
	// signal and therefore signals must be blocked for the duration of
	// rehighlight().
	dataEdit->blockSignals(true);
	highlighter->parseBlocks();
	highlighter->rehighlight();
	dataEdit->blockSignals(false);

	// Bundle updates by restarting the timer every time the data changes
	updateTimer->start(200);
}

void EmvViewerMainWindow::on_tagsCheckBox_stateChanged(int state)
{
	// Rehighlight when emphasis state changes. Note that rehighlight() will
	// also re-trigger the textChanged() signal and therefore signals must be
	// blocked for the duration of rehighlight().
	dataEdit->blockSignals(true);
	highlighter->setEmphasiseTags(state != Qt::Unchecked);
	highlighter->rehighlight();
	dataEdit->blockSignals(false);
}

void EmvViewerMainWindow::on_paddingCheckBox_stateChanged(int state)
{
	// Rehighlight when padding state changes. Note that rehighlight() will
	// also trigger the textChanged() signal which will in turn update the tree
	// view item associated with invalid data or padding as well.
	highlighter->setIgnorePadding(state != Qt::Unchecked);
	highlighter->rehighlight();

	// Note that tree view data must be reparsed when padding state changes
	treeView->setIgnorePadding(state != Qt::Unchecked);
	parseData();
}

void EmvViewerMainWindow::on_decodeCheckBox_stateChanged(int state)
{
	treeView->setDecodeFields(state != Qt::Unchecked);
}

void EmvViewerMainWindow::on_treeView_itemPressed(QTreeWidgetItem* item, int column)
{
	if (item->type() == EmvTreeItemType) {
		EmvTreeItem* etItem = reinterpret_cast<EmvTreeItem*>(item);

		// Highlight selected item in input data. Note that rehighlight() will
		// also trigger the textChanged() signal and therefore signals must be
		// blocked for the duration of rehighlight().
		dataEdit->blockSignals(true);
		highlighter->setSelection(
			etItem->srcOffset() * 2,
			etItem->srcLength() * 2
		);
		highlighter->rehighlight();
		dataEdit->blockSignals(false);

		// Show description of selected item
		// Assume that a tag description always has a tag name
		descriptionText->clear();
		if (!etItem->tagName().isEmpty()) {
			descriptionText->appendHtml(
				QStringLiteral("<b>") +
				etItem->tagName() +
				QStringLiteral("</b><br/><br/>") +
				etItem->tagDescription()
			);
		}

		// Let description scroll to top after updating content
		QTimer::singleShot(0, [this]() {
			descriptionText->verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
		});

		return;
	}

	displayLegal();
}

void EmvViewerMainWindow::on_descriptionText_linkActivated(const QString& link)
{
	// Open link using external application
	QDesktopServices::openUrl(link);
}
