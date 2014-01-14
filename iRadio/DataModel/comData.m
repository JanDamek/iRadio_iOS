//
//  comData.m
//  iRadioHD
//
//  Created by Jan Damek on 13.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import "comData.h"
#import <CoreData/CoreData.h>
#import "comAppDelegate.h"

@interface comData()

@property (strong, nonatomic, getter = getManagedObjectContext) NSManagedObjectContext *managedObjectContext;

@end

@implementation comData

@synthesize categories = _categories;
@synthesize radios = _radios;
@synthesize streams = _streams;

@synthesize favorites = _favorites;
@synthesize listeneds = _listeneds;

@synthesize managedObjectContext = _managedObjectContext;


-(NSManagedObjectContext *)getManagedObjectContext{
    if (!_managedObjectContext){
        comAppDelegate *d = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
        _managedObjectContext = d.managedObjectContext;
    }
    return _managedObjectContext;
}

- (NSFetchedResultsController *)getCategories{
    if (!_categories){
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        // Edit the entity name as appropriate.
        NSEntityDescription *entity = [NSEntityDescription entityForName:@"Categorie" inManagedObjectContext:self.managedObjectContext];
        [fetchRequest setEntity:entity];
        
        // Set the batch size to a suitable number.
        [fetchRequest setFetchBatchSize:20];
        
        // Edit the sort key as appropriate.
        NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"title" ascending:NO];
        NSArray *sortDescriptors = @[sortDescriptor];
        
        [fetchRequest setSortDescriptors:sortDescriptors];
        
        // Edit the section name key path and cache name if appropriate.
        // nil for section name key path means "no sections".
        NSFetchedResultsController *aFetchedResultsController = [[NSFetchedResultsController alloc] initWithFetchRequest:fetchRequest managedObjectContext:self.managedObjectContext sectionNameKeyPath:nil cacheName:@"Master"];
        //aFetchedResultsController.delegate = self;
        _categories = aFetchedResultsController;
        
        NSError *error = nil;
        if (![self.categories performFetch:&error]) {
            // Replace this implementation with code to handle the error appropriately.
            // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        }
    }
    
    return _categories;
}
-(NSFetchedResultsController *)getRadios{
    if (!_radios){
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        // Edit the entity name as appropriate.
        NSEntityDescription *entity = [NSEntityDescription entityForName:@"Radio" inManagedObjectContext:self.managedObjectContext];
        [fetchRequest setEntity:entity];
        
        // Set the batch size to a suitable number.
        [fetchRequest setFetchBatchSize:20];
        
        // Edit the sort key as appropriate.
        NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"name" ascending:NO];
        NSArray *sortDescriptors = @[sortDescriptor];
        
        [fetchRequest setSortDescriptors:sortDescriptors];
        
        // Edit the section name key path and cache name if appropriate.
        // nil for section name key path means "no sections".
        NSFetchedResultsController *aFetchedResultsController = [[NSFetchedResultsController alloc] initWithFetchRequest:fetchRequest managedObjectContext:self.managedObjectContext sectionNameKeyPath:nil cacheName:@"Master"];
        //aFetchedResultsController.delegate = self;
        _radios = aFetchedResultsController;
        
        NSError *error = nil;
        if (![self.radios performFetch:&error]) {
            // Replace this implementation with code to handle the error appropriately.
            // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        }
    }
    
    return _radios;
}

-(NSFetchedResultsController *)getStreams{
    if (!_streams){
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        // Edit the entity name as appropriate.
        NSEntityDescription *entity = [NSEntityDescription entityForName:@"Stream" inManagedObjectContext:self.managedObjectContext];
        [fetchRequest setEntity:entity];
        
        // Set the batch size to a suitable number.
        [fetchRequest setFetchBatchSize:20];
        
        // Edit the sort key as appropriate.
        NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"def3g" ascending:NO];
        NSArray *sortDescriptors = @[sortDescriptor];
        
        [fetchRequest setSortDescriptors:sortDescriptors];
        
        // Edit the section name key path and cache name if appropriate.
        // nil for section name key path means "no sections".
        NSFetchedResultsController *aFetchedResultsController = [[NSFetchedResultsController alloc] initWithFetchRequest:fetchRequest managedObjectContext:self.managedObjectContext sectionNameKeyPath:nil cacheName:@"Master"];
        //aFetchedResultsController.delegate = self;
        _streams = aFetchedResultsController;
        
        NSError *error = nil;
        if (![self.streams performFetch:&error]) {
            // Replace this implementation with code to handle the error appropriately.
            // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        }
    }
    
    return _streams;
}

-(NSFetchedResultsController *)getListeneds{
    if (!_listeneds){
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        // Edit the entity name as appropriate.
        NSEntityDescription *entity = [NSEntityDescription entityForName:@"Listened" inManagedObjectContext:self.managedObjectContext];
        [fetchRequest setEntity:entity];
        
        // Set the batch size to a suitable number.
        [fetchRequest setFetchBatchSize:20];
        
        // Edit the sort key as appropriate.
        NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"timeStamp" ascending:NO];
        NSArray *sortDescriptors = @[sortDescriptor];
        
        [fetchRequest setSortDescriptors:sortDescriptors];
        
        // Edit the section name key path and cache name if appropriate.
        // nil for section name key path means "no sections".
        NSFetchedResultsController *aFetchedResultsController = [[NSFetchedResultsController alloc] initWithFetchRequest:fetchRequest managedObjectContext:self.managedObjectContext sectionNameKeyPath:nil cacheName:@"Master"];
        //aFetchedResultsController.delegate = self;
        _listeneds = aFetchedResultsController;
        
        NSError *error = nil;
        if (![self.listeneds performFetch:&error]) {
            // Replace this implementation with code to handle the error appropriately.
            // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        }
    }
    
    return _listeneds;
}

-(NSFetchedResultsController *)getFavorites{
    if (!_favorites){
        NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
        // Edit the entity name as appropriate.
        NSEntityDescription *entity = [NSEntityDescription entityForName:@"Favorite" inManagedObjectContext:self.managedObjectContext];
        [fetchRequest setEntity:entity];
        
        // Set the batch size to a suitable number.
        [fetchRequest setFetchBatchSize:20];
        
        // Edit the sort key as appropriate.
        NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"timeStamp" ascending:NO];
        NSArray *sortDescriptors = @[sortDescriptor];
        
        [fetchRequest setSortDescriptors:sortDescriptors];
        
        // Edit the section name key path and cache name if appropriate.
        // nil for section name key path means "no sections".
        NSFetchedResultsController *aFetchedResultsController = [[NSFetchedResultsController alloc] initWithFetchRequest:fetchRequest managedObjectContext:self.managedObjectContext sectionNameKeyPath:nil cacheName:@"Master"];
        //aFetchedResultsController.delegate = self;
        _favorites = aFetchedResultsController;
        
        NSError *error = nil;
        if (![self.favorites performFetch:&error]) {
            // Replace this implementation with code to handle the error appropriately.
            // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        }
    }
    
    return _favorites;
}

-(void)saveContext:(NSManagedObjectContext *)context{
    // Save the context.
    NSError *error = nil;
    if (![context save:&error]) {
        // Replace this implementation with code to handle the error appropriately.
        // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
    
}

-(void)saveCategories{
    NSManagedObjectContext *context = [self.categories managedObjectContext];
    
    // Save the context.
    NSError *error = nil;
    if (![context save:&error]) {
        // Replace this implementation with code to handle the error appropriately.
        // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
}

-(void)saveRadios{
    NSManagedObjectContext *context = [self.radios managedObjectContext];
    
    // Save the context.
    NSError *error = nil;
    if (![context save:&error]) {
        // Replace this implementation with code to handle the error appropriately.
        // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
}

-(void)saveStreams{
    NSManagedObjectContext *context = [self.streams managedObjectContext];
    
    // Save the context.
    NSError *error = nil;
    if (![context save:&error]) {
        // Replace this implementation with code to handle the error appropriately.
        // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
}

-(void)saveListeneds{
    NSManagedObjectContext *context = [self.listeneds managedObjectContext];
    
    // Save the context.
    NSError *error = nil;
    if (![context save:&error]) {
        // Replace this implementation with code to handle the error appropriately.
        // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
}

-(void)saveFavorites{
    NSManagedObjectContext *context = [self.favorites managedObjectContext];
    
    // Save the context.
    NSError *error = nil;
    if (![context save:&error]) {
        // Replace this implementation with code to handle the error appropriately.
        // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
}

-(Categorie *)newCategorie{
    NSManagedObjectContext *context = [self.categories managedObjectContext];
    NSEntityDescription *entity = [[self.categories fetchRequest] entity];
    Categorie *newManagedObject = [NSEntityDescription insertNewObjectForEntityForName:[entity name] inManagedObjectContext:context];
    newManagedObject.timeStamp = [NSDate date];
    return newManagedObject;
}

-(Radio *)newRadio{
    NSManagedObjectContext *context = [self.radios managedObjectContext];
    NSEntityDescription *entity = [[self.radios fetchRequest] entity];
    Radio *newManagedObject = [NSEntityDescription insertNewObjectForEntityForName:[entity name] inManagedObjectContext:context];
    newManagedObject.timeStamp = [NSDate date];
    return newManagedObject;
}

-(Stream *)newStream{
    NSManagedObjectContext *context = [self.streams managedObjectContext];
    NSEntityDescription *entity = [[self.streams fetchRequest] entity];
    Stream *newManagedObject = [NSEntityDescription insertNewObjectForEntityForName:[entity name] inManagedObjectContext:context];
    newManagedObject.timeStamp = [NSDate date];
    return newManagedObject;
    
}

-(Favorite *)newFavorie{
    NSManagedObjectContext *context = [self.favorites managedObjectContext];
    NSEntityDescription *entity = [[self.favorites fetchRequest] entity];
    Favorite *newManagedObject = [NSEntityDescription insertNewObjectForEntityForName:[entity name] inManagedObjectContext:context];
    newManagedObject.timeStamp = [NSDate date];
    return newManagedObject;
}

-(Listened *)newListened{
    NSManagedObjectContext *context = [self.listeneds managedObjectContext];
    NSEntityDescription *entity = [[self.listeneds fetchRequest] entity];
    Listened *newManagedObject = [NSEntityDescription insertNewObjectForEntityForName:[entity name] inManagedObjectContext:context];
    newManagedObject.timeStamp = [NSDate date];
    return newManagedObject;
}

-(Categorie*)findCategorieId:(int)categorie_id{
    [self.categories.fetchRequest setPredicate:[NSPredicate predicateWithFormat:@"category_id==%i", categorie_id]];
    id <NSFetchedResultsSectionInfo> fc = self.categories.sections[0];
    for (Categorie *c in [fc objects]) {
        if ([c.category_id isEqualToNumber:[NSNumber numberWithInt:categorie_id]]){
            return c;
        }
    }

    return [self newCategorie];
}

-(Radio*)findRadioId:(int)radio_id{
    [self.radios.fetchRequest setPredicate:[NSPredicate predicateWithFormat:@"radio_id==%i", radio_id]];
    
    for (Radio *r in [self.radios.sections[0] objects]) {
        if ([r.radio_id isEqualToNumber:[NSNumber numberWithInt:radio_id]]){
            return r;
        }
    }
    
    return [self newRadio];
}

-(Stream*)findStreamId:(int)stream_id{
    [self.streams.fetchRequest setPredicate:[NSPredicate predicateWithFormat:@"stream_id==%i", stream_id]];
    
    for (Stream *s in [self.streams.sections[0] objects]) {
        if ([s.stream_id isEqualToNumber:[NSNumber numberWithInt:stream_id]]){
            return s;
        }
    }
    
    return [self newStream];
}


@end
