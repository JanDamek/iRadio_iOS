//
//  comData.h
//  iRadioHD
//
//  Created by Jan Damek on 13.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import "Categorie.h"
#import "Radio.h"
#import "Stream.h"
#import "Listened.h"
#import "Favorite.h"

@interface comData : NSObject

@property (strong, nonatomic, getter = getCategories) NSFetchedResultsController *categories;
@property (strong, nonatomic, getter = getRadios) NSFetchedResultsController *radios;
@property (strong, nonatomic, getter = getStreams) NSFetchedResultsController *streams;

@property (strong, nonatomic, getter = getFavorites) NSFetchedResultsController *favorites;
@property (strong, nonatomic, getter = getListeneds) NSFetchedResultsController *listeneds;

-(Categorie*)newCategorie;
-(Radio*)newRadio;
-(Stream*)newStream;

-(Listened*)newListened;
-(Favorite*)newFavorie;

-(void)saveContext:(NSManagedObjectContext*)context;

-(void)saveCategories;
-(void)saveRadios;
-(void)saveStreams;
-(void)saveListeneds;
-(void)saveFavorites;

-(Categorie*)findCategorieId:(int)categorie_id;
-(Radio*)findRadioId:(int)radio_id;
-(Stream*)findStreamId:(int)stream_id;

@end
