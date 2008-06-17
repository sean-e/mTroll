#include "AboutDlg.h"

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

	mLayout = new QBoxLayout(QBoxLayout::LeftToRight, this);
	mExitButton = new QPushButton("&Close", this);
	mLabel = new QLabel(labelTxt, this);

	mLabel->setAlignment(Qt::AlignCenter);
	mLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	mLabel->setOpenExternalLinks(true);

	mLayout->setSizeConstraint(QLayout::SetFixedSize);
	mLayout->addWidget(mLabel, 0, Qt::AlignCenter);
	mLayout->addWidget(mExitButton, 0, Qt::AlignBottom);

	connect(mExitButton, SIGNAL(clicked()), SLOT(reject()));
}

AboutDlg::~AboutDlg()
{
	delete mLayout;
	delete mLabel;
}
