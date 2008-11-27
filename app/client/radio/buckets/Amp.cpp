/***************************************************************************
 *   Copyright 2005-2008 Last.fm Ltd.                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "Amp.h"
#include "widgets/RadioControls.h"
#include "widgets/UnicornWidget.h"
#include "widgets/UnicornVolumeSlider.h"
#include "the/radio.h"
#include "widgets/ImageButton.h"
#include "PlayableListItem.h"
#include "PlayerBucketList.h"

#include <QPaintEvent>
#include <QPainter>
#include <QTimeLine>

struct BorderedContainer : public QWidget
{
    BorderedContainer( QWidget* parent = 0 ) : QWidget( parent ), m_showText( false )
    { 
        (new QHBoxLayout( this ))->setContentsMargins( 1, 1, 1, 1 );
        setAutoFillBackground( false ); 
        QPalette p = palette();
        p.setBrush( QPalette::Text, QBrush( 0x777777));
        setPalette( p );
    }
    
    void paintEvent( QPaintEvent* e )
    {
        QPainter p( this );
        p.setClipRect( e->rect() );
        p.setRenderHint( QPainter::Antialiasing );
        
        p.setPen( Qt::transparent );
        p.setBrush( palette().brush( QPalette::Window ));
        p.drawRoundedRect( rect(), 6, 6 );
        p.setPen( QPen( palette().brush( QPalette::Shadow ), 0));
        p.drawRoundedRect( rect(), 6, 6 );

        p.setPen( QPen( palette().brush( QPalette::Text).color()) );
        if( m_showText )
        {
		#ifndef WIN32
			QFont f = p.font();
		
            f.setBold( true );
            f.setPointSize( 12 );
			p.setFont( f );
		#endif
            
            p.drawText( rect(), 
                       Qt::AlignCenter | Qt::TextWordWrap, 
                       m_text );
        }
    }
    
    void showText( bool b ){ m_showText = b; update();}
    
    void resizeEvent( QResizeEvent* e )
    {
        QLinearGradient window( 0, 0, 0, e->size().height() );
        window.setColorAt( 0, Qt::black );
        window.setColorAt( 1, 0x2b2929 );
        
        QLinearGradient shadow( 0, 0, 0, e->size().height() );
        shadow.setColorAt( 0, 0x0d0c0c );
        shadow.setColorAt( 0.5, 0x1c1b1b );
        shadow.setColorAt( 1, 0x5e5e5e );
        
        QPalette p = palette();
        p.setBrush( QPalette::Window, window );
        p.setBrush( QPalette::Shadow, shadow );
        setPalette( p );
    }
    
    void setText( const QString& s ){ m_text = s; update(); }
    
private: QString m_text; bool m_showText;
};


Amp::Amp()
{
    setupUi();
    
    connect( qApp, SIGNAL(trackSpooled( const Track&, StopWatch*)), SLOT( onTrackSpooled(const Track&, StopWatch*)) );
    connect( qApp, SIGNAL(stateChanged( State, Track)), SLOT( onStateChanged(  State, Track)) );
    connect( qApp, SIGNAL(playerChanged( QString )), SLOT( onPlayerChanged( QString )));
    
    onPlayerBucketChanged();
}


void 
Amp::setupUi()
{
    new QHBoxLayout( this );
    
    layout()->setSpacing( 13 );
    layout()->setContentsMargins( 11, 9, 12, 11 );
    
    {
        QVBoxLayout* v = new QVBoxLayout;
        v->addStretch();
        v->addWidget( ui.dashboardButton = new ImageButton( QPixmap(":/Amp/button/dashboard/rest.png") ) );
        v->addWidget( ui.bucketsButton = new ImageButton( QPixmap(":/Amp/button/buckets/rest.png") ) );
        v->addStretch();
        v->setMargin( 0 );
        v->setSpacing( 0 );
        ((QBoxLayout*)layout())->addLayout( v );
        
        QPixmap p1( ":/Amp/button/buckets/checked.png" );
        QPixmap p2( ":/Amp/button/dashboard/checked.png" );
//        ui.bucketsButton->setPixmap( p1, QIcon::On );
//        ui.dashboardButton->setPixmap( p2, QIcon::On );
        ui.bucketsButton->setPixmap( p1, QIcon::Off, QIcon::Active );
        ui.dashboardButton->setPixmap( p2, QIcon::Off, QIcon::Active );
    }
    
    ui.borderWidget = new BorderedContainer( this );
    
    {
        //Radio and volume controls need to be embedded in a parent QWidget
        //in order to properly mask when animating out / into view.
        QWidget* controls = new QWidget(ui.borderWidget);
        new QHBoxLayout( controls );
        controls->layout()->setSpacing( 0 );
        controls->layout()->setContentsMargins( 0, 0, 0, 0 );
        controls->layout()->addWidget( ui.controls = new RadioControls );
        ui.borderWidget->layout()->addWidget( controls );
    }
    {
        QWidget* volume = new QWidget(ui.borderWidget);
        new QHBoxLayout( volume );
        volume->layout()->setSpacing( 0 );
        volume->layout()->setContentsMargins( 0, 0, 0, 0 );
        volume->layout()->addWidget( ui.volume = new UnicornVolumeSlider );
        ui.borderWidget->layout()->addWidget( volume );
    }
    
    ui.controls->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred ) );
    
    ((QBoxLayout*)ui.borderWidget->layout())->insertWidget( 1, ui.bucket = new PlayerBucketList( ui.borderWidget ));
    layout()->addWidget( ui.borderWidget );
        
    connect( ui.bucket, SIGNAL( itemAdded( QString, Seed::Type)), SLOT( onPlayerBucketChanged()));
    connect( ui.bucket, SIGNAL( itemRemoved( QString, Seed::Type)), SLOT( onPlayerBucketChanged()));

    ui.volume->setAudioOutput( The::radio().audioOutput());
    ui.volume->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred ) );

    connect( ui.bucket, SIGNAL( itemRemoved( QString, Seed::Type)), SIGNAL( itemRemoved( QString, Seed::Type)));
    
    connect( ui.controls, SIGNAL( skip()), &The::radio(), SLOT( skip()) );
    connect( ui.controls, SIGNAL( stop()), &The::radio(), SLOT( stop()));
    connect( ui.controls, SIGNAL( play()), ui.bucket, SLOT( play()));

    setFixedHeight( 86 );
    setAutoFillBackground( true );
}


void 
Amp::onPlayerBucketChanged()
{
    if( ui.bucket->count() > 0 )
    {
        showWidgetAnimated( ui.controls, Left );
        showWidgetAnimated( ui.volume, Right );
        ui.borderWidget->showText( false );
    }
    else
    {
        hideWidgetAnimated( ui.controls, Left );
        hideWidgetAnimated( ui.volume, Right );
        if( m_playerState != Paused )
            ui.borderWidget->showText( true );
    }
}


void 
Amp::addAndLoadItem( const QString& itemText, const Seed::Type type )
{
    PlayableListItem* item = new PlayableListItem;
    item->setText( itemText );
    item->setPlayableType( type );
	item->setForeground( Qt::white );
	item->setBackground( QColor( 0x2e, 0x2e, 0x2e));
	item->setFlags( item->flags() | Qt::ItemIsDragEnabled );
    item->fetchImage();
    ui.bucket->addItem( item );
    
}


void 
Amp::resizeEvent( QResizeEvent* event )
{
    QPalette p = palette();
    QLinearGradient window( 0, 0, 0, event->size().height() );
    window.setColorAt( 0, 0x383737 );
    window.setColorAt( 1, 0x161616 );
    p.setBrush( QPalette::Window, window );
    setPalette( p );
    
    if( event->size().width() == event->oldSize().width() )
        return;
    
    if( ui.bucket->count() == 0 )
    {
        ui.controls->move( -ui.controls->rect().width(), 0);
        ui.volume->move( ui.volume->rect().width(), 0);
    }
        
}


void 
Amp::paintEvent( QPaintEvent* event )
{
    QPainter p( this );
    p.setClipRect( event->rect() );
    
    p.setPen( Qt::black );
    p.drawLine( rect().topLeft(), rect().topRight() );
    p.setPen( 0x4c4b4b );
    p.drawLine( rect().topLeft() + QPoint(0,1), rect().topRight() + QPoint(0,1) );
    p.setPen( Qt::black );
    p.drawLine( rect().bottomLeft(), rect().bottomRight() );
}




void 
Amp::showWidgetAnimated( QWidget* w, AnimationPosition p )
{
    if( w->pos().x() == 0 )
        return;
    
    QTimeLine* tl = new QTimeLine( 500, w );
    tl->setUpdateInterval( 10 );
    switch( p )
    {
        case Left:       
            tl->setFrameRange( -w->rect().width(), 0 );
            break;
        case Right:
            tl->setFrameRange( w->rect().width(), 0 );
            break;
        default:
            Q_ASSERT( !"Unknown animation position" );
            break;
    }
    connect( tl, SIGNAL( frameChanged( int )), this, SLOT( onWidgetAnimationFrameChanged( int )));
    connect( tl, SIGNAL( finished()), tl, SLOT( deleteLater()));
    tl->start();
}


void 
Amp::hideWidgetAnimated( QWidget* w, AnimationPosition p )
{
    if( w->pos().x() > 0 || w->pos().x() < 0 )
        return;

    QTimeLine* tl = new QTimeLine( 500, w );
    tl->setUpdateInterval( 10 );
    switch( p )
    {
        case Left:       
            tl->setFrameRange( 0, -w->rect().width() );
            break;
        case Right:
            tl->setFrameRange( 0, w->rect().width() );
            break;
        default:
            Q_ASSERT( !"Unknown animation position" );
            break;
    }

    connect( tl, SIGNAL( frameChanged( int )), this, SLOT( onWidgetAnimationFrameChanged( int )));
    connect( tl, SIGNAL( finished()), tl, SLOT( deleteLater()));
    tl->start();
}


void 
Amp::onWidgetAnimationFrameChanged( int frame )
{
    QTimeLine* tl = qobject_cast<QTimeLine*>(sender());
    QWidget* w = qobject_cast<QWidget*>(tl->parent());
    if( !w )
        return;
    
    w->move( frame, w->pos().y());
}


void 
Amp::onTrackSpooled( const Track& t, StopWatch* )
{
    if( t.source() != Track::LastFmRadio )
    {
        if( t.isNull() )
            ui.borderWidget->showText( false );
        else if( ui.bucket->count() == 0 )
        {
            ui.borderWidget->showText( true );
        }
    }
}


void 
Amp::onStateChanged( State s, const Track& t )
{
    m_playerState = s;
    if( t.source() != Track::LastFmRadio && s == Paused )
        ui.borderWidget->showText( false );
    else if( s == Playing && ui.bucket->count() == 0 )
        ui.borderWidget->showText( true );
}


void 
Amp::onPlayerChanged( const QString& name )
{
    ui.borderWidget->setText( tr( "%1 is now playing." ).arg( name ) );
    ui.borderWidget->showText( false );
    m_playerState = Paused;
}
