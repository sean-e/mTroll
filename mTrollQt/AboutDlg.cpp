#include "AboutDlg.h"
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QFrame>
#include <QGroupBox>

extern QString GetBuildDate();


AboutDlg::AboutDlg()
{
	setWindowTitle(tr("About mTroll"));
	QString labelTxt =
		"<html><body>mTroll MIDI Controller<br><br>"
		"<a href=\"http://www.creepingfog.com/mTroll/\">http://www.creepingfog.com/mTroll/</a><br><br>"
		"&copy; copyright 2007-2008 Sean Echevarria<br><br>";
	labelTxt += "Built " + ::GetBuildDate();
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

	connect(mExitButton, SIGNAL(clicked()), SLOT(reject()));
}

AboutDlg::~AboutDlg()
{
	delete mExitButton;
	delete mLabel;
	delete mLabelGroupBox;
	delete mLayout;
}
