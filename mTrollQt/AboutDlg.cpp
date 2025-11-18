/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2018,2020,2021-2025 Sean Echevarria
 *
 * This file is part of mTroll.
 *
 * mTroll is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mTroll is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Let me know if you modify, extend or use mTroll.
 * Original project site: http://www.creepingfog.com/mTroll/
 * SourceForge site: http://sourceforge.net/projects/mtroll/
 * Contact Sean: "fester" at the domain of the original project site
 */

#include "AboutDlg.h"
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QFrame>
#include <QGroupBox>

extern QString GetBuildDate();


AboutDlg::AboutDlg(QWidget * parent) : QDialog(parent)
{
	setWindowTitle(tr("About mTroll"));
	QString labelTxt =
		"<html><body>mTroll MIDI Controller<br><br>"
		"<a href=\"http://www.creepingfog.com/mTroll/\">http://www.creepingfog.com/mTroll/</a><br><br>"
		"<a href=\"https://github.com/sean-e/mtroll/\">https://github.com/sean-e/mtroll/</a><br><br>"
		"&copy; copyright 2007-2025 Sean Echevarria<br><br>"
		"MIDI beat clock thread &copy; copyright 2016 Pete Brown<br><br>";
	labelTxt += "Built " + ::GetBuildDate() + 
#ifdef _M_X64
		" (x86-64)"
#else
	#ifdef _M_IX86
			" (x86-32)"
	#else
		#ifdef _M_ARM64
				" (ARM64)"
		#else
		#endif
	#endif
#endif
		"<br><br>";
	labelTxt += "Uses the open source <a href=\"https://www.qt.io/download-open-source\">Qt</a> framework licensed under GPL v3";
	labelTxt += "</body></html>";

	mExitButton = new QPushButton("&Close", this);

	mLabel = new QLabel(labelTxt, this);
	mLabel->setAlignment(Qt::AlignCenter);
	mLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	mLabel->setOpenExternalLinks(true);

	QVBoxLayout * labelLayout = new QVBoxLayout;
	labelLayout->addWidget(mLabel);
	labelLayout->addStretch(1);

	mLabelGroupBox = new QGroupBox("", this);
	mLabelGroupBox->setLayout(labelLayout); // takes ownership

	mLayout = new QBoxLayout(QBoxLayout::LeftToRight, this);
	mLayout->setSizeConstraint(QLayout::SetFixedSize);
	mLayout->addWidget(mLabelGroupBox, 0, Qt::AlignCenter);
	mLayout->addWidget(mExitButton, 0, Qt::AlignBottom);

	connect(mExitButton, &QPushButton::clicked, this, &QDialog::reject);
}

AboutDlg::~AboutDlg()
{
	delete mExitButton;
	delete mLabel;
	delete mLabelGroupBox;
	delete mLayout;
}
