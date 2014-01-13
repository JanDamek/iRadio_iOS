//
//  Radio.h
//  iRadio
//
//  Created by Jan Damek on 12.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Categorie, Favorite;

@interface Radio : NSManagedObject

@property (nonatomic, retain) NSString * descript;
@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) NSSet *stream_rel;
@property (nonatomic, retain) NSSet *categorie_rel;
@property (nonatomic, retain) Favorite *favorited_rel;
@property (nonatomic, retain) NSManagedObject *listened_rel;
@end

@interface Radio (CoreDataGeneratedAccessors)

- (void)addStream_relObject:(NSManagedObject *)value;
- (void)removeStream_relObject:(NSManagedObject *)value;
- (void)addStream_rel:(NSSet *)values;
- (void)removeStream_rel:(NSSet *)values;

- (void)addCategorie_relObject:(Categorie *)value;
- (void)removeCategorie_relObject:(Categorie *)value;
- (void)addCategorie_rel:(NSSet *)values;
- (void)removeCategorie_rel:(NSSet *)values;

@end
